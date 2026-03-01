#pragma once
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>

struct optiq 
{
    // optiq header
    __u8 version;
    __u8 optiq_length;
    __u16 service_id;
    __u32 session_id;
    __u32 sequence_number;
    __u64 timestamp;
    // SBE payload
    __u16 message_type;
    __u16 quantity;
    __u32 price;
    __u64 order_id;
} __attribute__((packed));

struct Packet {

    struct ethhdr eth;
    struct iphdr ip;
    struct udphdr udp;

    struct optiq bs; 

}__attribute__((packed));
