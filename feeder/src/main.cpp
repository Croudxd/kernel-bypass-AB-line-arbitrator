#include "../include/packet_util.hpp"
#include <iostream>
#include <chrono>
#include <linux/if_link.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_xdp.h>
#include <xdp/xsk.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <arpa/inet.h>

int main()
{
    const uint32_t NUM_FRAMES = 4096;
    const uint32_t FRAME_SIZE = 2048;
    void* umem_buffer = nullptr;

    if (posix_memalign(&umem_buffer, getpagesize(), NUM_FRAMES * FRAME_SIZE)) return 1;

    struct xsk_umem_config umem_cfg = {};
    umem_cfg.fill_size = 2048;
    umem_cfg.comp_size = 2048;
    umem_cfg.frame_size = FRAME_SIZE;
    umem_cfg.frame_headroom = XDP_PACKET_HEADROOM;
    umem_cfg.flags = 0;

    struct xsk_ring_prod fill_ring;
    struct xsk_ring_cons comp_ring;
    struct xsk_umem *umem;

    if (xsk_umem__create(&umem, umem_buffer, NUM_FRAMES * FRAME_SIZE, &fill_ring, &comp_ring, &umem_cfg)) return 1;

    struct xsk_socket_config xsk_cfg = {};
    xsk_cfg.rx_size = 2048;
    xsk_cfg.tx_size = 2048;
    xsk_cfg.xdp_flags = 0;
    xsk_cfg.bind_flags = XDP_USE_NEED_WAKEUP;
    xsk_cfg.libbpf_flags = 0;

    struct xsk_ring_cons rx_ring;
    struct xsk_ring_prod tx_ring;
    struct xsk_socket *xsk;

    if (xsk_socket__create(&xsk, "vethA_src", 0, umem, &rx_ring, &tx_ring, &xsk_cfg)) return 1;

    int xsk_fd = xsk_socket__fd(xsk);

    uint32_t global_seq = 1;
    int count = 0;

    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed >= 10)
        {
            std::cout << "10 seconds up\n";
            break;
        }

        uint32_t BATCH_SIZE = 64;
        uint32_t idx;

        if (xsk_ring_prod__reserve(&tx_ring, BATCH_SIZE, &idx) == BATCH_SIZE)
        {
            for (uint32_t i = 0; i < BATCH_SIZE; i++)
            {
                uint64_t frame_addr = ((count + i) % NUM_FRAMES) * FRAME_SIZE;

                struct xdp_desc *desc = xsk_ring_prod__tx_desc(&tx_ring, idx + i);
                desc->addr = frame_addr;
                desc->len = sizeof(Packet);

                Packet* pkt = (Packet*)((uint8_t*)umem_buffer + frame_addr);

                packet_util::set_packet(pkt);
                pkt->bs = packet_util::rand_struct();
                pkt->bs.sequence_number = htonl(global_seq++);
            }

            xsk_ring_prod__submit(&tx_ring, BATCH_SIZE);
            sendto(xsk_fd, NULL, 0, MSG_DONTWAIT, NULL, 0);

            count += BATCH_SIZE;
            sched_yield();
        }

        uint32_t comp_idx;
        uint32_t completed = xsk_ring_cons__peek(&comp_ring, 2048, &comp_idx);
        if (completed > 0)
        {
            xsk_ring_cons__release(&comp_ring, completed);
        }
            
        count++;
    }

    std::cout << "total sent: " << count << "\n";
    std::cout << "per second: " << count / 10 << "\n";

    return 0;
}
