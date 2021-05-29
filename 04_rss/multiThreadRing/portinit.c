#include "portinit.h"

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
    .mq_mode = ETH_MQ_RX_RSS,
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            //mlx5 support hf:
            .rss_hf = ETH_RSS_IP | ETH_RSS_TCP | ETH_RSS_UDP,
            //i40e support hf: 
            //.rss_hf = ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_NONFRAG_IPV4_TCP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    }

};

int port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = NUM_RX_QUEUE, tx_rings = NUM_TX_QUEUE;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0)
    {
        printf("Error during getting device (port %u) info: %s\n",
               port, strerror(-retval));
        return retval;
    }
    printf("\n\ninitializing port %d...\n", port);

    if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_CHECKSUM)
    {
        printf("port[%u] support RX cheksum offload.\n", port);
        port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_CHECKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    {
        printf("port[%u] support TX mbuf fast free offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MT_LOCKFREE)
    {
        printf("port[%u] support TX MT lock free offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MT_LOCKFREE;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM)
    {
        printf("port[%u] support TX IPv4 checksum offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM)
    {
        printf("port[%u] support TX UDP checksum offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM)
    {
        printf("port[%u] support TX TCP checksum offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_TCP_CKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_SCTP_CKSUM)
    {
        printf("port[%u] support TX SCTP checksum offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_SCTP_CKSUM;
    }

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up multiple RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up multiple TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0) 
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    struct rte_eth_link link;
    do
    {
        retval = rte_eth_link_get_nowait(port, &link);
        if (retval < 0)
        {
            printf("Failed link get (port %u): %s\n",
                   port, rte_strerror(-retval));
            return retval;
        }
        else if (link.link_status)
            break;

        printf("Waiting for Link up on port %" PRIu16 "\n", port);
        sleep(1);
    } while (!link.link_status);

    /* Display the port MAC address. */
    struct rte_ether_addr addr;
    retval = rte_eth_macaddr_get(port, &addr);
    if (retval != 0)
        return retval;

    printf("Port[%u] MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
           ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 "\n",
           port,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    retval = rte_eth_promiscuous_enable(port);
    if (retval != 0)
        return retval;

    return 0;
}
