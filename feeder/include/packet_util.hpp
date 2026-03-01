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
        __u32 current_id;
        __u32 current_id_2;
        static uint64_t fast_rand_state;

    public:
        static void set_packet(Packet* packet, unsigned char eth_dest[], unsigned char eth_src[], const char* src_ip, const char* dest_ip);
        static uint16_t calculate_ip_checksum(struct iphdr* ip);
        static uint64_t xorshift64();
        static void rand_struct(Packet* pack);
};
