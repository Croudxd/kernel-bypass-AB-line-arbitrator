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
#include <sched.h>
#include <errno.h>

int main()
{
    const uint32_t NUM_FRAMES = 4096;
    const uint32_t FRAME_SIZE = 2048;

    void* umem_buffer_A = nullptr;
    void* umem_buffer_B = nullptr;

    if (posix_memalign(&umem_buffer_A, getpagesize(), NUM_FRAMES * FRAME_SIZE)) return 1;
    if (posix_memalign(&umem_buffer_B, getpagesize(), NUM_FRAMES * FRAME_SIZE)) return 1;

    struct xsk_umem_config umem_cfg = {};
    umem_cfg.fill_size = 2048;
    umem_cfg.comp_size = 2048;
    umem_cfg.frame_size = FRAME_SIZE;
    umem_cfg.frame_headroom = XDP_PACKET_HEADROOM;
    umem_cfg.flags = 0;

    struct xsk_ring_prod fill_ring_A, fill_ring_B;
    struct xsk_ring_cons comp_ring_A, comp_ring_B;
    struct xsk_umem *umem_A, *umem_B;

    if (xsk_umem__create(&umem_A, umem_buffer_A, NUM_FRAMES * FRAME_SIZE, &fill_ring_A, &comp_ring_A, &umem_cfg)) return 1;
    if (xsk_umem__create(&umem_B, umem_buffer_B, NUM_FRAMES * FRAME_SIZE, &fill_ring_B, &comp_ring_B, &umem_cfg)) return 1;

    struct xsk_socket_config xsk_cfg = {};
    xsk_cfg.rx_size = 2048;
    xsk_cfg.tx_size = 2048;
    xsk_cfg.xdp_flags = 0;
    xsk_cfg.bind_flags = XDP_COPY;
    xsk_cfg.libbpf_flags = 0;

    struct xsk_ring_cons rx_ring_A, rx_ring_B;
    struct xsk_ring_prod tx_ring_A, tx_ring_B;
    struct xsk_socket *xsk_A, *xsk_B;

    if (xsk_socket__create(&xsk_A, "vethA_src", 0, umem_A, &rx_ring_A, &tx_ring_A, &xsk_cfg)) return 1;
    if (xsk_socket__create(&xsk_B, "vethB_src", 0, umem_B, &rx_ring_B, &tx_ring_B, &xsk_cfg)) return 1;

    int xsk_fd_A = xsk_socket__fd(xsk_A);
    int xsk_fd_B = xsk_socket__fd(xsk_B);

    uint32_t global_seq = 1;
    uint64_t count = 0;

    for (uint32_t i = 0; i < NUM_FRAMES; i++) {
        Packet* pA = (Packet*)((uint8_t*)umem_buffer_A + (i * FRAME_SIZE));
        Packet* pB = (Packet*)((uint8_t*)umem_buffer_B + (i * FRAME_SIZE));
        packet_util::set_packet(pA, pB);
    }

    const uint32_t BATCH_SIZE = 512;

    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 10) break;
        uint32_t comp_idx_A, comp_idx_B;
        uint32_t completed_A = xsk_ring_cons__peek(&comp_ring_A, 2048, &comp_idx_A);
        if (completed_A > 0) xsk_ring_cons__release(&comp_ring_A, completed_A);

        uint32_t completed_B = xsk_ring_cons__peek(&comp_ring_B, 2048, &comp_idx_B);
        if (completed_B > 0) xsk_ring_cons__release(&comp_ring_B, completed_B);

        bool space_A = xsk_prod_nb_free(&tx_ring_A, BATCH_SIZE) >= BATCH_SIZE;
        bool space_B = xsk_prod_nb_free(&tx_ring_B, BATCH_SIZE) >= BATCH_SIZE;

        if (space_A && space_B) {
            uint32_t idx_A, idx_B;
            xsk_ring_prod__reserve(&tx_ring_A, BATCH_SIZE, &idx_A);
            xsk_ring_prod__reserve(&tx_ring_B, BATCH_SIZE, &idx_B);

            for (uint32_t i = 0; i < BATCH_SIZE; i++) {
                uint64_t frame_addr = ((count + i) % NUM_FRAMES) * FRAME_SIZE;

                struct xdp_desc *d_A = xsk_ring_prod__tx_desc(&tx_ring_A, idx_A + i);
                d_A->addr = frame_addr;
                d_A->len = sizeof(Packet);

                struct xdp_desc *d_B = xsk_ring_prod__tx_desc(&tx_ring_B, idx_B + i);
                d_B->addr = frame_addr;
                d_B->len = sizeof(Packet);

                Packet* pkt_A = (Packet*)((uint8_t*)umem_buffer_A + frame_addr);
                Packet* pkt_B = (Packet*)((uint8_t*)umem_buffer_B + frame_addr);

                uint32_t seq = htonl(global_seq++);
                pkt_A->bs.sequence_number = seq;
                pkt_B->bs.sequence_number = seq;
            }

            xsk_ring_prod__submit(&tx_ring_A, BATCH_SIZE);
            xsk_ring_prod__submit(&tx_ring_B, BATCH_SIZE);
            count += BATCH_SIZE;
        }

        if (sendto(xsk_fd_A, NULL, 0, MSG_DONTWAIT, NULL, 0) < 0 && errno != EAGAIN)
            perror("sendto A");
        if (sendto(xsk_fd_B, NULL, 0, MSG_DONTWAIT, NULL, 0) < 0 && errno != EAGAIN)
            perror("sendto B");
    }

    std::cout << "Total sent per line: " << count << "\n";
    std::cout << "PPS per line:        " << count / 10 << "\n";

    xsk_socket__delete(xsk_A);
    xsk_socket__delete(xsk_B);
    xsk_umem__delete(umem_A);
    xsk_umem__delete(umem_B);
    free(umem_buffer_A);
    free(umem_buffer_B);

    return 0;
}
