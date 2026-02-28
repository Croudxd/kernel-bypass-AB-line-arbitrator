#include "../include/packet_util.hpp"
#include <cstring>
#include <netinet/in.h>

int16_t packet_util::current_id = 0;
uint64_t packet_util::fast_rand_state = 88172645463325252ULL;

void packet_util::set_packet(Packet* packet_a, Packet* packet_b)
{
    unsigned char eth_dest_a[] = { 0x06,0x4c,0x54,0xeb,0x06,0x9d };
    unsigned char eth_src_a[] = { 0xf2,0x68,0x7f,0xcc,0x7f,0x57 };

    memcpy(packet_a->eth.h_dest, eth_dest_a, 6);
    memcpy(packet_a->eth.h_source, eth_src_a, 6);
    packet_a->eth.h_proto = htons(ETH_P_IP);

    packet_a->ip.ihl = 5;
    packet_a->ip.version = 4;
    packet_a->ip.tos = 0x10;
    packet_a->ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(optiq));
    packet_a->ip.id = htons(current_id++);
    packet_a->ip.frag_off = htons(0x4000);
    packet_a->ip.ttl = 64;
    packet_a->ip.protocol = 17;
    packet_a->ip.saddr = inet_addr("192.168.1.10");
    packet_a->ip.daddr = inet_addr("192.168.1.20");
    void* ip_ptr = &(packet_a->ip);
    packet_a->ip.check = calculate_ip_checksum((struct iphdr*)ip_ptr);

    packet_a->udp.source = htons(0x1F90);
    packet_a->udp.dest = htons(0x1F90);
    packet_a->udp.len = htons(sizeof(struct udphdr) + sizeof(optiq));
    packet_a->udp.check = 0;

    memcpy(packet_b->eth.h_dest, eth_dest_a, 6);
    memcpy(packet_b->eth.h_source, eth_src_a, 6);
    packet_b->eth.h_proto = htons(ETH_P_IP);

    packet_b->ip.ihl = 5;
    packet_b->ip.version = 4;
    packet_b->ip.tos = 0x10;
    packet_b->ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(optiq));
    packet_b->ip.id = htons(current_id++);
    packet_b->ip.frag_off = htons(0x4000);
    packet_b->ip.ttl = 64;
    packet_b->ip.protocol = 17;
    packet_b->ip.saddr = inet_addr("192.168.1.10");
    packet_b->ip.daddr = inet_addr("192.168.1.20");
    void* ip_ptr_b = &(packet_b->ip);
    packet_b->ip.check = calculate_ip_checksum((struct iphdr*)ip_ptr_b);

    packet_b->udp.source = htons(0x1F90);
    packet_b->udp.dest = htons(0x1F90);
    packet_b->udp.len = htons(sizeof(struct udphdr) + sizeof(optiq));
    packet_b->udp.check = 0;
}

uint16_t packet_util::calculate_ip_checksum(struct iphdr* ip)
{
    ip->check = 0;
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)ip;

    for (int i = 0; i < 10; i++)
    {
        sum += ptr[i];
    }

    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

uint64_t packet_util::xorshift64() {
    fast_rand_state ^= fast_rand_state << 13;
    fast_rand_state ^= fast_rand_state >> 7;
    fast_rand_state ^= fast_rand_state << 17;
    return fast_rand_state;
}

optiq packet_util::rand_struct()
{
    optiq msg;
    uint64_t r = xorshift64();
    uint64_t r2 = xorshift64();

    msg.version         = static_cast<uint8_t>((r % 4) + 1);
    msg.optiq_length    = static_cast<uint8_t>((r % 17) + 20);
    msg.service_id      = static_cast<uint16_t>((r >> 8) % 100 + 1);
    msg.session_id      = static_cast<uint32_t>((r >> 16) % 9000 + 1000);

    msg.sequence_number = static_cast<uint32_t>(r % 10000000 + 1);

    msg.timestamp       = (r2 % (1700000000000000ULL - 1600000000000000ULL)) + 1600000000000000ULL;

    msg.message_type    = static_cast<uint16_t>((r2 >> 8) % 10 + 1);
    msg.quantity        = static_cast<uint16_t>((r2 >> 16) % 500 + 1);
    msg.price           = static_cast<uint32_t>((r2 >> 32) % 40001 + 10000);
    msg.order_id        = (xorshift64() % 999999999ULL) + 1;

    return msg;
}
