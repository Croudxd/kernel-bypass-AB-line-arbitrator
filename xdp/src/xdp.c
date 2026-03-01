#include <linux/bpf.h>
#include <linux/bpf_common.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "../../common/optiq.h"

char LICENSE[] SEC("license") = "GPL";
/**
 *Get data and data_end from ctx
Check ethernet header fits and protocol is IPv4
Check IP header fits and protocol is UDP
Check UDP header fits
Check your struct fits
Read session_id from your struct
Look it up in the BPF map
Seen before → DROP, not seen → add to map and PASS
 *  Steps:
 *  1. Create a eBPF map.
 *  2. Get packet, check session_id against eBPF map.
 *  3. Either drop or pass the packet.
 *
 */


struct
{
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32);    // session_id
    __type(value, __u8);   // just a flag, 1 = seen
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} seen_sessions SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
    __uint(pinning, LIBBPF_PIN_BY_NAME);
} xsks_map SEC(".maps");

const int RETURN_CODE = XDP_DROP;

SEC("xdp")
int xdp_main(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    struct ethhdr *eth = data;
    __u64 network_header_offset = sizeof(*eth);

    if (data + network_header_offset > data_end)
    {
        return RETURN_CODE;
    }
    __u16 h_proto = eth->h_proto;

    if (h_proto != bpf_htons(ETH_P_IP))
    {
        return XDP_PASS;
    }

    struct iphdr *ip = data + sizeof(*eth);
    network_header_offset += sizeof(*ip);

    if (data + network_header_offset > data_end)
    {
      return RETURN_CODE;
    }

    __u8 protocol = ip->protocol;
    if ( protocol != IPPROTO_UDP)
    {
        return XDP_PASS;
    }

    struct udphdr *udp = data + sizeof(*eth) + sizeof(*ip);
    network_header_offset += sizeof(*udp);

    if (data + network_header_offset > data_end)
    {
        return RETURN_CODE;
    }

    struct optiq *op = data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);

    network_header_offset += sizeof(*op);

    if (data + network_header_offset > data_end)
    {
        return RETURN_CODE;
    }

    __u32 *val = bpf_map_lookup_elem(&seen_sessions, &op->session_id);
    if (val)
    {
        return RETURN_CODE;
    }
    else 
    {
        __u8 seen = 1;
        bpf_map_update_elem(&seen_sessions, &op->session_id, &seen, BPF_ANY);
    }

    return bpf_redirect_map(&xsks_map, 0, 0);
}


