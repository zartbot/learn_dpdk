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

typedef struct
{
    uint64_t code;
    const char *name;
} offload_capa_t;

static offload_capa_t rx_capa_name[] = {
    {DEV_RX_OFFLOAD_VLAN_STRIP, "DEV_RX_OFFLOAD_VLAN_STRIP"},
    {DEV_RX_OFFLOAD_IPV4_CKSUM, "DEV_RX_OFFLOAD_IPV4_CKSUM"},
    {DEV_RX_OFFLOAD_UDP_CKSUM, "DEV_RX_OFFLOAD_UDP_CKSUM"},
    {DEV_RX_OFFLOAD_TCP_CKSUM, "DEV_RX_OFFLOAD_TCP_CKSUM"},
    {DEV_RX_OFFLOAD_TCP_LRO, "DEV_RX_OFFLOAD_TCP_LRO"},
    {DEV_RX_OFFLOAD_QINQ_STRIP, "DEV_RX_OFFLOAD_QINQ_STRIP"},
    {DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM, "DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM"},
    {DEV_RX_OFFLOAD_MACSEC_STRIP, "DEV_RX_OFFLOAD_MACSEC_STRIP"},
    {DEV_RX_OFFLOAD_HEADER_SPLIT, "DEV_RX_OFFLOAD_HEADER_SPLIT"},
    {DEV_RX_OFFLOAD_VLAN_FILTER, "DEV_RX_OFFLOAD_VLAN_FILTER"},
    {DEV_RX_OFFLOAD_VLAN_EXTEND, "DEV_RX_OFFLOAD_VLAN_EXTEND"},
    {DEV_RX_OFFLOAD_JUMBO_FRAME, "DEV_RX_OFFLOAD_JUMBO_FRAME"},
    {DEV_RX_OFFLOAD_SCATTER, "DEV_RX_OFFLOAD_SCATTER"},
    {DEV_RX_OFFLOAD_TIMESTAMP, "DEV_RX_OFFLOAD_TIMESTAMP"},
    {DEV_RX_OFFLOAD_SECURITY, "DEV_RX_OFFLOAD_SECURITY"},
    {DEV_RX_OFFLOAD_KEEP_CRC, "DEV_RX_OFFLOAD_KEEP_CRC"},
    {DEV_RX_OFFLOAD_SCTP_CKSUM, "DEV_RX_OFFLOAD_SCTP_CKSUM"},
    {DEV_RX_OFFLOAD_OUTER_UDP_CKSUM, "DEV_RX_OFFLOAD_OUTER_UDP_CKSUM"},
    {DEV_RX_OFFLOAD_RSS_HASH, "DEV_RX_OFFLOAD_RSS_HASH"}};

static offload_capa_t tx_capa_name[] = {
    {DEV_TX_OFFLOAD_VLAN_INSERT, "DEV_TX_OFFLOAD_VLAN_INSERT"},
    {DEV_TX_OFFLOAD_IPV4_CKSUM, "DEV_TX_OFFLOAD_IPV4_CKSUM"},
    {DEV_TX_OFFLOAD_UDP_CKSUM, "DEV_TX_OFFLOAD_UDP_CKSUM"},
    {DEV_TX_OFFLOAD_TCP_CKSUM, "DEV_TX_OFFLOAD_TCP_CKSUM"},
    {DEV_TX_OFFLOAD_SCTP_CKSUM, "DEV_TX_OFFLOAD_SCTP_CKSUM"},
    {DEV_TX_OFFLOAD_TCP_TSO, "DEV_TX_OFFLOAD_TCP_TSO"},
    {DEV_TX_OFFLOAD_UDP_TSO, "DEV_TX_OFFLOAD_UDP_TSO"},
    {DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM, "DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM"},
    {DEV_TX_OFFLOAD_QINQ_INSERT, "DEV_TX_OFFLOAD_QINQ_INSERT"},
    {DEV_TX_OFFLOAD_VXLAN_TNL_TSO, "DEV_TX_OFFLOAD_VXLAN_TNL_TSO"},
    {DEV_TX_OFFLOAD_GRE_TNL_TSO, "DEV_TX_OFFLOAD_GRE_TNL_TSO"},
    {DEV_TX_OFFLOAD_IPIP_TNL_TSO, "DEV_TX_OFFLOAD_IPIP_TNL_TSO"},
    {DEV_TX_OFFLOAD_GENEVE_TNL_TSO, "DEV_TX_OFFLOAD_GENEVE_TNL_TSO"},
    {DEV_TX_OFFLOAD_MACSEC_INSERT, "DEV_TX_OFFLOAD_MACSEC_INSERT"},
    {DEV_TX_OFFLOAD_MT_LOCKFREE, "DEV_TX_OFFLOAD_MT_LOCKFREE"},
    {DEV_TX_OFFLOAD_MULTI_SEGS, "DEV_TX_OFFLOAD_MULTI_SEGS"},
    {DEV_TX_OFFLOAD_MBUF_FAST_FREE, "DEV_TX_OFFLOAD_MBUF_FAST_FREE"},
    {DEV_TX_OFFLOAD_SECURITY, "DEV_TX_OFFLOAD_SECURITY"},
    {DEV_TX_OFFLOAD_UDP_TNL_TSO, "DEV_TX_OFFLOAD_UDP_TNL_TSO"},
    {DEV_TX_OFFLOAD_IP_TNL_TSO, "DEV_TX_OFFLOAD_IP_TNL_TSO"},
    {DEV_TX_OFFLOAD_OUTER_UDP_CKSUM, "DEV_TX_OFFLOAD_OUTER_UDP_CKSUM"},
    {DEV_TX_OFFLOAD_SEND_ON_TIMESTAMP, "DEV_TX_OFFLOAD_SEND_ON_TIMESTAMP"},
};

int main(int argc, char *argv[])
{

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    printf("\n\n\n*****************************************\n");

    int nb_ports, port_id;

    nb_ports = rte_eth_dev_count_avail();
    printf("number of available port: %d\n", nb_ports);

    struct rte_eth_dev_info dev_info;

    RTE_ETH_FOREACH_DEV(port_id)
    {
        printf("=========================================\n");

        /* get device info */
        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret != 0)
        {
            printf("Error during getting device (port %u) info: %s\n",
                   port_id, strerror(-ret));
            return ret;
        }

        printf("port: %d \tDriver:%s \n", port_id, dev_info.driver_name);
        struct rte_eth_link link;
        char link_status[RTE_ETH_LINK_MAX_STR_LEN];
        ret = rte_eth_link_get_nowait(port_id, &link);
        rte_eth_link_to_str(link_status, sizeof(link_status), &link);
        printf("%s\n", link_status);

        char port_name[RTE_ETH_NAME_MAX_LEN];
        ret = rte_eth_dev_get_name_by_port(port_id, port_name);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get port name: err=%d, port=%d\n", ret, port_id);

        static struct rte_ether_addr ports_eth_addr;
        ret = rte_eth_macaddr_get(port_id, &ports_eth_addr);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get MAC address: err=%d, port=%d\n", ret, port_id);

        char mac[18];
        rte_ether_format_addr(&mac[0], 18, &ports_eth_addr);
        printf("MAC address: %s\nPCIe:%s\n", mac, port_name);

        printf("Max RX Queue: \t%d\tDesc:\t%d\n", dev_info.max_rx_queues, dev_info.rx_desc_lim.nb_max);
        printf("Max TX Queue: \t%d\tDesc:\t%d\n", dev_info.max_tx_queues, dev_info.tx_desc_lim.nb_max);

        printf("Offload Capability:\n");
        /* check offload capability */
        uint64_t rx_capa = dev_info.rx_offload_capa;
        uint32_t num = sizeof(rx_capa_name) / sizeof(offload_capa_t);
        for (int i = 0; i < num; ++i)
        {
            if (rx_capa & rx_capa_name[i].code)
            {
                printf("  %s\n", rx_capa_name[i].name);
            }
        }
        printf("-----------------------------------------\n");

        uint64_t tx_capa = dev_info.tx_offload_capa;
        num = sizeof(tx_capa_name) / sizeof(offload_capa_t);
        for (int i = 0; i < num; ++i)
        {
            if (tx_capa & tx_capa_name[i].code)
            {
                printf("  %s\n", tx_capa_name[i].name);
            }
        }

        printf("=========================================\n\n");
    }
    return 0;
}
