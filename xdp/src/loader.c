#include <xdp/libxdp.h>
#include <net/if.h>

int main()
{
    {
        int ifindex = if_nametoindex("vethA_dst");
        struct xdp_program *prog = xdp_program__open_file("xdp.o", "xdp", NULL);
        xdp_program__attach(prog, ifindex, XDP_MODE_SKB, 0);
    }
    {
        int ifindex = if_nametoindex("vethB_dst");
        struct xdp_program *prog = xdp_program__open_file("xdp.o", "xdp", NULL);
        xdp_program__attach(prog, ifindex, XDP_MODE_SKB, 0);
    }
    return 0;
}
