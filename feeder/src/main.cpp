#include "binary_struct.hpp"
#include "packet_util.hpp"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <linux/if_link.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_xdp.h>
#include <xdp/xsk.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <arpa/inet.h>
#include <sched.h>
#include <errno.h>

constexpr uint16_t PAGE_SIZE = 4096;
constexpr uint16_t NUM_FRAMES = 2048;

int main()
{
    int count = 0;

    auto start_time = std::chrono::steady_clock::now();

    auto* bufs = mmap(NULL, NUM_FRAMES * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,  -1, 0);

	if (bufs == MAP_FAILED) 
    {
		printf("ERROR: mmap failed\n");
		exit(EXIT_FAILURE);
	}

    xsk_ring_prod fill_ring{};
    xsk_ring_cons comp_ring{};
    xsk_ring_prod tx_ring{};
    struct xsk_umem_config umem_cfg{};
    umem_cfg.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS * 2;
    umem_cfg.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    umem_cfg.frame_size = PAGE_SIZE;
    umem_cfg.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM;

    struct xsk_umem *umem;
    int uret = xsk_umem__create(&umem, bufs, NUM_FRAMES * PAGE_SIZE, &fill_ring, &comp_ring, &umem_cfg);
    if (uret) {
        printf("ERROR: xsk_umem__create failed: %s\n", strerror(-uret));
        exit(EXIT_FAILURE);
    }

    __u32 idx;
    xsk_socket *sock;

    struct xsk_socket_config cfg{};
    cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    cfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;
    cfg.bind_flags = XDP_COPY;
	int ret;

	ret = xsk_socket__create(&sock, "vethA_src", 0, umem, nullptr, &tx_ring, &cfg);
    if (ret) {
        printf("ERROR: xsk_socket__create failed: %s\n", strerror(-ret));
        exit(EXIT_FAILURE);
    }

    while (true) 
    {
        auto current_time = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 10) break;
        Packet a{};
        Packet b{};
        packet_util::set_packet(&a,&b);

        uint32_t tx_idx;
        memcpy(xsk_umem__get_data(bufs, 0), &a, sizeof(Packet));
        xsk_ring_prod__reserve(&tx_ring, 1, &tx_idx);
        struct xdp_desc * desc = xsk_ring_prod__tx_desc(&tx_ring, tx_idx);
        desc->addr = 0;
        desc->len = sizeof(Packet);

        xsk_ring_prod__submit(&tx_ring, 1);
        sendto(xsk_socket__fd(sock), NULL, 0, MSG_DONTWAIT, NULL, 0);
        uint32_t comp_idx;
        uint32_t completed = xsk_ring_cons__peek(&comp_ring, 2048, &comp_idx);
        if (completed > 0)
            xsk_ring_cons__release(&comp_ring, completed);
        count++;
    }

    std::cout << "Total sent per line: " << count << "\n";
    std::cout << "PPS per line:        " << count / 10 << "\n";

    return 0;
}
