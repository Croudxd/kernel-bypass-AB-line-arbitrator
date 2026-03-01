#pragma once

#include "../../common/optiq.h"
#include <cstdint>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <arpa/inet.h>

class packet_util
{
    private:
        inline static __u32 current_id;
        inline static __u32 current_id_2;
        inline static uint64_t fast_rand_state = 88172645463325252ULL;

    public:
        static void set_packet(Packet* packet, unsigned char eth_dest[], unsigned char eth_src[], const char* src_ip, const char* dest_ip);
        static uint16_t calculate_ip_checksum(struct iphdr* ip);
        static uint64_t xorshift64();
        static void rand_struct(Packet* pack);
};
