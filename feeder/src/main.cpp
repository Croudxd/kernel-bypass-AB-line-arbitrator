#include "xdp.hpp"
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

static unsigned char src_a[] = {0xf2,0x68,0x7f,0xcc,0x7f,0x57};
static unsigned char dest_a[] = {0x06,0x4c,0x54,0xeb,0x06,0x9d};
static const char* ip_src_a = "192.168.1.10";
static const char* ip_dest_a = "192.168.1.20";

 static unsigned char src_b[] = {0x72,0xc6,0xb3,0x65,0xc5,0x40};
 static unsigned char dest_b[] = {0xb6,0x93,0xf6,0x05,0x52,0xc9};
 static const char* ip_src_b = "192.168.2.10";
 static const char* ip_dest_b = "192.168.2.20";


int main()
{
    int count = 0;

    auto start_time = std::chrono::steady_clock::now();


    xdp_feeder xdp_a;
    xdp_a.xdp_setup("vethA_src");

    xdp_feeder xdp_b;
    xdp_b.xdp_setup("vethB_src");

    while (true) 
    {
        auto current_time = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 10) break;

        xdp_a.xdp_send(src_a, dest_a, ip_src_a, ip_dest_a);
        xdp_b.xdp_send(src_b, dest_b, ip_src_b, ip_dest_b);
        count++;
    }

    std::cout << "Total sent per line: " << count << "\n";
    std::cout << "PPS per line:        " << count / 10 << "\n";

    return 0;
}
