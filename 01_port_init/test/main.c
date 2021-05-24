#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <pthread.h>
#include <string.h>

#define MAX_PORTS 16

int main(int argc, char *argv[])
{

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    printf("\n\n\n*****************************************\n");

    int nb_ports;
    nb_ports = rte_eth_dev_count_avail();
    printf("number of available port: %d\n", nb_ports);

    struct rte_eth_dev_info dev_info;
    for (int portid = 0; portid < nb_ports; ++portid)
    {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get device info: err=%d, port=%d\n", ret, portid);

        printf("port: %d Driver:%s\n", portid, dev_info.driver_name);
    }

    /* ethernet addresses of ports */
    static struct rte_ether_addr ports_eth_addr[MAX_PORTS];
    for (int portid = 0; portid < nb_ports; ++portid)
    {
        ret = rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get MAC address: err=%d, port=%d\n", ret, portid);

        char mac[18];
        rte_ether_format_addr(&mac[0], 18, &ports_eth_addr[portid]);
        printf("port: %d->MAC-> %s\n", portid, mac);
    }

    return 0;
}