#pragma once
#include <cstdint>

struct binary_struct 
{
    // optiq header
    uint8_t version;
    uint8_t optiq_length;
    uint16_t service_id;
    uint32_t session_id;
    uint32_t sequence_number;
    uint64_t timestamp;
    // SBE payload
    
    uint16_t message_type;
    uint16_t quantity;
    uint32_t price;
    uint64_t order_id;
}__attribute__((packed));
