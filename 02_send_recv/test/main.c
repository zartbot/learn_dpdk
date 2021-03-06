#include <stdint.h>
#include <unistd.h>
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
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    }
};

static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
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

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up 1 TX queue per Ethernet port. */
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

rte_be32_t string_to_ip(char *s)
{
    unsigned char a[4];
    int rc = sscanf(s, "%hhd.%hhd.%hhd.%hhd", a + 0, a + 1, a + 2, a + 3);
    if (rc != 4)
    {
        fprintf(stderr, "bad source IP address format. Use like: 1.1.1.1\n");
        exit(1);
    }
    return (rte_be32_t)(a[3]) << 24 |
           (rte_be32_t)(a[2]) << 16 |
           (rte_be32_t)(a[1]) << 8 |
           (rte_be32_t)(a[0]);
}

static int
lcore_send(struct rte_mempool *mbuf_pool) {

    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_udp_hdr *udp_hdr;

    //Defined header in UDP
    struct SRoU
    {
        uint8_t magic_num;
        uint8_t srou_length;
        uint8_t flags;
        uint8_t next_protcol;
        uint64_t pad;
    };

    //init mac
    struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    struct rte_ether_addr d_addr = {{0x3c, 0xfd, 0xfe, 0xa9, 0xa8, 0x89}};

    //init IP header
    rte_be32_t s_ip_addr = string_to_ip("1.0.0.253");
    rte_be32_t d_ip_addr = string_to_ip("1.0.0.1");
    uint16_t ether_type = rte_cpu_to_be_16(0x0800);

    //init udp payload
    struct SRoU obj = {
        .magic_num = 1,
        .srou_length = 4,
        .flags = 0xFF,
        .next_protcol = 0,
    };
    struct SRoU *msg;
    struct rte_mbuf *pkt[BURST_SIZE];

    for (int i = 0; i < BURST_SIZE; i++)
    {
        pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
        eth_hdr = rte_pktmbuf_mtod(pkt[i], struct rte_ether_hdr *);
        eth_hdr->d_addr = d_addr;
         struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, i}};
        eth_hdr->s_addr = s_addr;
        eth_hdr->ether_type = ether_type;

        ipv4_hdr = rte_pktmbuf_mtod_offset(pkt[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        ipv4_hdr->version_ihl = 0x45;
        ipv4_hdr->next_proto_id = 0x11;
        ipv4_hdr->src_addr = s_ip_addr;
        ipv4_hdr->dst_addr = d_ip_addr;
        ipv4_hdr->time_to_live = 0x40;

        udp_hdr = rte_pktmbuf_mtod_offset(pkt[i], struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
        udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr));
        udp_hdr->src_port = rte_cpu_to_be_16(1234);
        udp_hdr->dst_port = rte_cpu_to_be_16(6666);
        ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr) + sizeof(struct rte_ipv4_hdr));

        msg = (struct SRoU *)(rte_pktmbuf_mtod(pkt[i], char *) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr));
        *msg = obj;
        int pkt_size = sizeof(struct SRoU) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);

        pkt[i]->l2_len = sizeof(struct rte_ether_hdr);
        pkt[i]->l3_len = sizeof(struct rte_ipv4_hdr);
        pkt[i]->l4_len = sizeof(struct rte_udp_hdr);
        pkt[i]->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
        ipv4_hdr->hdr_checksum = 0;
        udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr, pkt[i]->ol_flags);
        pkt[i]->data_len = pkt_size;
        pkt[i]->pkt_len = pkt_size;
    }


    for(;;) {
    uint16_t nb_tx = rte_eth_tx_burst(0, 0, pkt, BURST_SIZE);
    printf("successful send %d pkts\n", nb_tx);
    sleep(2);

    }
    for (int i = 0; i < BURST_SIZE; i++)
    {
        rte_pktmbuf_free(pkt[i]);
    }

}

int main(int argc, char *argv[])
{

    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    printf("\n\n\n*****************************************\n");

    nb_ports = rte_eth_dev_count_avail();
    printf("number of available port: %d\n", nb_ports);

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);

    /* start packet send function on lcore-1 */
    rte_eal_remote_launch((lcore_function_t *)lcore_send,mbuf_pool,1);

    struct rte_mbuf *rx_pkt[BURST_SIZE];
    for (int i = 0; i < BURST_SIZE; i++)
    {
        rx_pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
    }

    for (;;)
    {
        uint16_t nb_rx = rte_eth_rx_burst(1, 0, rx_pkt, BURST_SIZE);
        if (nb_rx == 0)
        {
            continue;
        }
        struct rte_ether_hdr *eth_hdr;
        for (int i = 0; i < nb_rx; i++)
        {
            eth_hdr = rte_pktmbuf_mtod(rx_pkt[i], struct rte_ether_hdr *);
            printf("Recv Pkt[%d] from MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
                   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " \n",i,
                   eth_hdr->s_addr.addr_bytes[0], eth_hdr->s_addr.addr_bytes[1],
                   eth_hdr->s_addr.addr_bytes[2], eth_hdr->s_addr.addr_bytes[3],
                   eth_hdr->s_addr.addr_bytes[4], eth_hdr->s_addr.addr_bytes[5]);
            rte_pktmbuf_free(rx_pkt[i]);
        }
    }
    return 0;
}