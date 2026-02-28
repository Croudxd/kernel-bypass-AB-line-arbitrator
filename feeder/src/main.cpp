#include <iostream>
#include <chrono>
#include <random>
#include <sys/socket.h>

#include "../include/binary_struct.hpp"
#include "../include/udp.hpp"

binary_struct rand_struct( std::mt19937 gen )
{
        std::uniform_int_distribution<uint16_t> version_dist(1, 4);
        std::uniform_int_distribution<uint16_t> length_dist(20, 36);
        std::uniform_int_distribution<uint16_t> service_dist(1, 100);
        std::uniform_int_distribution<uint32_t> session_dist(1000, 9999);
        std::uniform_int_distribution<uint32_t> seq_dist(1, 10000000);
        std::uniform_int_distribution<uint64_t> ts_dist(1600000000000000ULL, 1700000000000000ULL);
        
        std::uniform_int_distribution<uint16_t> msg_type_dist(1, 10);
        std::uniform_int_distribution<uint16_t> qty_dist(1, 500);
        std::uniform_int_distribution<uint32_t> price_dist(10000, 50000);
        std::uniform_int_distribution<uint64_t> order_id_dist(1, 999999999ULL);

        binary_struct msg;

        msg.version = static_cast<uint8_t>(version_dist(gen));
        msg.optiq_length = static_cast<uint8_t>(length_dist(gen));
        msg.service_id = service_dist(gen);
        msg.session_id = session_dist(gen);
        msg.sequence_number = seq_dist(gen);
        msg.timestamp = ts_dist(gen);
        
        msg.message_type = msg_type_dist(gen);
        msg.quantity = qty_dist(gen);
        msg.price = price_dist(gen);
        msg.order_id = order_id_dist(gen);
        return msg;
}

int main()
{
    auto start_time = std::chrono::steady_clock::now();
    std::random_device rd;
    std::mt19937 gen(rd());

    int count = 0;

    udp u;
    u.connect();
    while (true)
    {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed >= 10) 
        {
            std::cout << "10 seconds up\n";
            break;
        }
        auto bs = rand_struct(gen);
        u.send(&bs);
        count++;
    }
    u.udpclose();
    std::cout << sizeof(binary_struct) << std::endl;
    std::cout << "per second:"<< count / 10 << std::endl;
    return 0;
}


