#include "../include/packet_util.hpp"
#include <cstring>
#include <netinet/in.h>

int16_t packet_util::current_id = 0;
uint64_t packet_util::fast_rand_state = 88172645463325252ULL;

void packet_util::set_packet(Packet* packet, unsigned char eth_dest[], unsigned char eth_src[], const char* src_ip, const char* dst_ip)
{
    memcpy(packet->eth.h_dest, eth_dest, 6);
    memcpy(packet->eth.h_source, eth_src, 6);
    packet->eth.h_proto = htons(ETH_P_IP);

    packet->ip.ihl = 5;
    packet->ip.version = 4;
    packet->ip.tos = 0x10;
    packet->ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(optiq));
    packet->ip.id = htons(current_id++);
    packet->ip.frag_off = htons(0x4000);
    packet->ip.ttl = 64;
    packet->ip.protocol = 17;
    packet->ip.saddr = inet_addr(src_ip);
    packet->ip.daddr = inet_addr(dst_ip);
    void* ip_ptr = &(packet->ip);
    packet->ip.check = calculate_ip_checksum((struct iphdr*)ip_ptr);

    packet->udp.source = htons(0x1F90);
    packet->udp.dest = htons(0x1F90);
    packet->udp.len = htons(sizeof(struct udphdr) + sizeof(optiq));
    packet->udp.check = 0;
    rand_struct(packet);

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

void packet_util::rand_struct(Packet* pack)
{
    optiq& msg = pack->bs;
    uint64_t r = xorshift64();
    uint64_t r2 = xorshift64();

    msg.version         = static_cast<uint8_t>((r % 4) + 1);
    msg.optiq_length    = static_cast<uint8_t>((r % 17) + 20);
    msg.service_id      = static_cast<uint16_t>((r >> 8) % 100 + 1);
    msg.session_id      = current_id++;

    msg.sequence_number = static_cast<uint32_t>(r % 10000000 + 1);

    msg.timestamp       = (r2 % (1700000000000000ULL - 1600000000000000ULL)) + 1600000000000000ULL;

    msg.message_type    = static_cast<uint16_t>((r2 >> 8) % 10 + 1);
    msg.quantity        = static_cast<uint16_t>((r2 >> 16) % 500 + 1);
    msg.price           = static_cast<uint32_t>((r2 >> 32) % 40001 + 10000);
    msg.order_id        = (xorshift64() % 999999999ULL) + 1;
}
