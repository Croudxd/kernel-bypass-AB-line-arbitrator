# kernel-bypass-AB-line-arbitrator

A high-performance market data line arbitrator using AF_XDP and eBPF. Receives duplicate market data feeds on two separate network lines (A and B), deduplicates packets at the NIC using an XDP hook, and delivers unique packets to userspace via kernel bypass.

---

## Overview

In financial trading infrastructure, market data is often delivered over two redundant network lines for reliability. This project implements a line arbitrator that:

- **Feeds** packets onto both lines simultaneously using AF_XDP zero-copy transmission
- **Deduplicates** packets at the NIC using an eBPF XDP hook and a shared BPF map
- **Receives** unique packets in userspace via AF_XDP socket redirect, bypassing the kernel networking stack

```
feeder → vethA_src → vethA_dst → XDP hook ─┐
                                             ├─→ shared seen_sessions map → userspace receiver
feeder → vethB_src → vethB_dst → XDP hook ─┘
```

The first packet with a given `session_id` is passed through to userspace. The duplicate arriving on the other line is dropped at the NIC — never touching the kernel networking stack or userspace.

---

## Architecture

### Packet Format

All packets use a custom `optiq` struct wrapped in standard Ethernet/IP/UDP headers:

```
[ Ethernet Header ][ IP Header ][ UDP Header ][ optiq payload ]
```

The `optiq` struct contains:

| Field | Type | Description |
|---|---|---|
| `version` | `__u8` | Protocol version |
| `optiq_length` | `__u8` | Payload length |
| `service_id` | `__u16` | Service identifier |
| `session_id` | `__u32` | Unique session identifier used for deduplication |
| `sequence_number` | `__u32` | Packet sequence number |
| `timestamp` | `__u64` | Nanosecond timestamp |
| `message_type` | `__u16` | SBE message type |
| `quantity` | `__u16` | Order quantity |
| `price` | `__u32` | Order price |
| `order_id` | `__u64` | Order identifier |

---

## Components

### 1. Feeder (`feeder/`)

Generates synthetic market data packets and transmits them onto both network lines at high throughput using AF_XDP.

#### How it works

Memory is allocated with `mmap` and registered as a UMEM — a flat buffer divided into fixed-size frames that both userspace and the kernel can access directly. An AF_XDP socket is created for each interface and bound to this UMEM.

To send a packet:

1. Write packet data into a UMEM frame
2. Reserve a slot in the TX ring
3. Write a descriptor pointing to the frame offset and length
4. Submit the descriptor
5. Kick the kernel with `sendto` to trigger transmission
6. Drain the completion ring to reclaim frames for reuse

The TX ring and completion ring form a producer/consumer pair. Without draining completions the TX ring fills up and transmission stalls — the kernel kick must happen unconditionally every loop iteration regardless of whether new packets were submitted.

#### Performance

Achieves ~160k–230k PPS per line on veth pairs. On real hardware with zero-copy mode (`XDP_ZEROCOPY`) throughput would be significantly higher.

---

### 2. XDP Hook (`xdp/`)

An eBPF XDP program that runs at the NIC driver level on the destination interfaces (`vethA_dst`, `vethB_dst`). Processes packets before they enter the kernel networking stack.

#### How it works

For each incoming packet the program:

1. Validates the Ethernet header and checks for IPv4
2. Validates the IP header and checks for UDP
3. Validates the UDP header
4. Bounds-checks the `optiq` payload
5. Extracts `session_id` from the payload
6. Looks up `session_id` in the `seen_sessions` BPF map
7. If found: `XDP_DROP` — duplicate, discard
8. If not found: adds to map, then `bpf_redirect_map` to userspace socket

#### BPF Maps

Two pinned maps are shared between both XDP program instances (A and B):

**`seen_sessions`** (`BPF_MAP_TYPE_LRU_HASH`) — tracks seen `session_id` values. LRU eviction prevents the map from filling up under sustained load. Key is `session_id` (`__u32`), value is a presence flag (`__u8`).

**`xsks_map`** (`BPF_MAP_TYPE_XSKMAP`) — holds the AF_XDP socket file descriptor for userspace redirect. The receiver inserts its socket fd into this map at startup. XDP calls `bpf_redirect_map` to deliver packets directly into the receiver's UMEM.

Both maps use `LIBBPF_PIN_BY_NAME` pinning so they persist at `/sys/fs/bpf/` and are shared across both XDP program instances on the two interfaces.

#### Compiling

```bash
clang -O2 -g -target bpf -c src/xdp.c -o xdp.o
```

#### Loading

```bash
sudo ip link set vethA_dst xdpgeneric obj xdp.o sec xdp
sudo ip link set vethB_dst xdpgeneric obj xdp.o sec xdp
```

---

### 3. Receiver (`receiver/`)

A userspace C++ program that receives deduplicated packets from the XDP hook via AF_XDP socket redirect, bypassing the kernel networking stack entirely.

#### How it works

The receiver creates a UMEM and an AF_XDP socket bound to `vethA_dst` in RX-only mode. It retrieves the pinned `xsks_map` from the BPF filesystem and inserts its socket fd — this tells the XDP hook where to redirect packets.

Before the receive loop, the fill ring is pre-populated with empty UMEM frames so the kernel has somewhere to write incoming packets immediately.

The receive loop:

1. Peek the RX ring for available descriptors
2. For each descriptor, read the UMEM offset
3. Cast directly to `Packet*` at that offset — zero copy, no memcpy
4. Process the packet
5. Release RX ring entries
6. Refill the fill ring with the consumed frames so the kernel can reuse them

The fill ring and RX ring form a producer/consumer pair for the receive path — analogous to the TX ring and completion ring on the send side.

---

## Ring Buffer Design

All data transfer between userspace and the kernel uses SPSC (single-producer, single-consumer) ring buffers. There are four rings per socket:

| Ring | Direction | Purpose |
|---|---|---|
| TX | userspace → kernel | Descriptors for packets to send |
| Completion | kernel → userspace | Notification that TX frames can be reused |
| Fill | userspace → kernel | Empty UMEM frames for the kernel to fill with RX data |
| RX | kernel → userspace | Descriptors for received packets |

For a TX-only socket (feeder) only TX and completion rings are used. For an RX-only socket (receiver) only RX and fill rings are used.

---

## Setup

### Prerequisites

- Linux kernel 5.10+
- `libxdp`, `libbpf`, `libelf`, `zlib`
- `clang` for BPF compilation
- Root privileges for XDP attachment and AF_XDP socket creation

### Network Setup

The project uses veth pairs to simulate two network lines:

```bash
sudo ip link add vethA_src type veth peer name vethA_dst
sudo ip link add vethB_src type veth peer name vethB_dst
sudo ip link set vethA_src up
sudo ip link set vethA_dst up
sudo ip link set vethB_src up
sudo ip link set vethB_dst up
```

Optionally add netem delay to line B to simulate a slower feed:

```bash
sudo tc qdisc add dev vethB_src root netem delay 1ms
```

### Building

```bash
# Build feeder
cd feeder && make

# Build receiver  
cd receiver && make

# Build XDP program
clang -O2 -g -target bpf -c xdp/src/xdp.c -o xdp/xdp.o
```

### Running

```bash
# 1. Load XDP program onto destination interfaces
sudo ip link set vethA_dst xdpgeneric obj xdp/xdp.o sec xdp
sudo ip link set vethB_dst xdpgeneric obj xdp/xdp.o sec xdp

# 2. Start receiver (terminal 1)
sudo ./receiver/receiver

# 3. Start feeder (terminal 2)
sudo ./feeder/main
```

### Cleanup

```bash
sudo ip link set vethA_dst xdp off
sudo ip link set vethB_dst xdp off
sudo rm -f /sys/fs/bpf/seen_sessions /sys/fs/bpf/xsks_map
```

---

## Notes

**Race condition**: There is a theoretical race window where the same `session_id` could arrive on both lines simultaneously, both pass the map lookup before either inserts, and both get forwarded to userspace. In practice this is negligible given real-world line latency differences, and the netem delay on line B makes it effectively impossible in the test environment. A `bpf_spin_lock` could eliminate this entirely at the cost of some latency.

**Copy mode**: The project uses `XDP_COPY` bind mode for compatibility with veth interfaces which do not support zero-copy. On real NICs with `XDP_ZEROCOPY` support, throughput would be significantly higher with no change to the application logic.

**Map eviction**: `seen_sessions` uses `BPF_MAP_TYPE_LRU_HASH` so old entries are automatically evicted under memory pressure, preventing the map from filling up and causing missed deduplication.
