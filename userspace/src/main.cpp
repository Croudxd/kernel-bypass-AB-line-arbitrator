/**
 *
 * 1. Create another link to the NIC.
 *
 *
 **/
#include "../../common/optiq.h"
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


static constexpr uint16_t PAGE_SIZE = 4096;
static constexpr uint16_t NUM_FRAMES = 2048;

int main()
{
    void * bufs; 

    xsk_ring_prod fill_ring{};
    xsk_ring_cons comp_ring{};
    xsk_ring_cons rx_ring{};

    struct xsk_umem_config umem_cfg{};
    struct xsk_umem *umem;

    const char* addr;
    addr = "vethA_dst";
    xsk_socket *sock;

    struct xsk_socket_config cfg{};

    bufs = mmap(NULL, NUM_FRAMES * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,  -1, 0);

    if (bufs == MAP_FAILED) 
    {
        printf("ERROR: mmap failed\n");
        exit(EXIT_FAILURE);
    }

    umem_cfg.fill_size = XSK_RING_PROD__DEFAULT_NUM_DESCS * 2;
    umem_cfg.comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    umem_cfg.frame_size = PAGE_SIZE;
    umem_cfg.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM;

    int uret = xsk_umem__create(&umem, bufs, NUM_FRAMES * PAGE_SIZE, &fill_ring, &comp_ring, &umem_cfg);
    if (uret) 
    {
        printf("ERROR: xsk_umem__create failed: %s\n", strerror(-uret));
        exit(EXIT_FAILURE);
    }

    cfg.libxdp_flags = XSK_LIBXDP_FLAGS__INHIBIT_PROG_LOAD;
    cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    cfg.bind_flags = XDP_COPY;
    cfg.tx_size = 0;
    int ret;

    ret = xsk_socket__create(&sock, addr, 0, umem, &rx_ring, nullptr,  &cfg);
    if (ret) {
        printf("ERROR: xsk_socket__create failed: %s\n", strerror(-ret));
        exit(EXIT_FAILURE);
    }

    int map = bpf_obj_get("/sys/fs/bpf/xsks_map");
    if (map < 0)
    {
        printf("ERROR: couldn't get xsks_map: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int sock_fd = xsk_socket__fd(sock);
    __u32 key = 0;

    bpf_map_update_elem(map, &key, &sock_fd, BPF_ANY);

    uint32_t fill_idx;
    xsk_ring_prod__reserve(&fill_ring, XSK_RING_PROD__DEFAULT_NUM_DESCS, &fill_idx);

    for (uint32_t i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i++)
        *xsk_ring_prod__fill_addr(&fill_ring, fill_idx + i) = i * PAGE_SIZE;

    xsk_ring_prod__submit(&fill_ring, XSK_RING_PROD__DEFAULT_NUM_DESCS);

    while (true)
    {
        uint32_t rx_idx;
        uint32_t completed = xsk_ring_cons__peek(&rx_ring, 2048, &rx_idx);

        for (uint32_t i = 0; i < completed; i++) 
        {
            const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&rx_ring, rx_idx + i);
            uint64_t addr = desc->addr;
            optiq *op = (optiq*)((uint8_t*)xsk_umem__get_data(bufs, addr) + sizeof(ethhdr) + sizeof(iphdr) + sizeof(udphdr));
            std::cout << op->session_id << std::endl;
        }

        if (completed > 0)
            xsk_ring_cons__release(&rx_ring, completed);

        uint32_t fill_idx;
        xsk_ring_prod__reserve(&fill_ring, completed, &fill_idx);

        for (uint32_t i = 0; i < completed; i++)
            *xsk_ring_prod__fill_addr(&fill_ring, fill_idx + i) =  i * PAGE_SIZE;

        xsk_ring_prod__submit(&fill_ring, completed);
    }

    return 0;
}
