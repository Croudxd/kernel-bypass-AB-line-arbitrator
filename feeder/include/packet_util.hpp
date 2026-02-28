#pragma once

#include "../include/binary_struct.hpp"
#include <cstdint>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <arpa/inet.h>

class packet_util
{
    private:
        static int16_t current_id;
        static uint64_t fast_rand_state;

    public:
        static void set_packet(Packet* packet);
        static uint16_t calculate_ip_checksum(struct iphdr* ip);
        static uint64_t xorshift64();
        static binary_struct rand_struct();
};
