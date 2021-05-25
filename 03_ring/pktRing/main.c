#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include "portinit.h"
#include "send_pkt.h"
#include "tx_ring.h"
#include "rx_ring.h"


static int
lcore_send(struct rte_mempool *mbuf_pool)
{
    struct rte_mbuf *pkt[BURST_SIZE];
    for (int i = 0; i < BURST_SIZE; i++)
    {
        pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
        build_pkt(pkt[i]);
    }
    for (;;)
    {
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
    struct rte_ring *tx_ring;
	struct rte_ring *rx_ring;
    unsigned nb_ports=2;
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");


    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	tx_ring = rte_ring_create("Output_ring", SCHED_TX_RING_SZ,
			rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
	if (tx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create output ring\n");

	rx_ring = rte_ring_create("Input_ring", SCHED_RX_RING_SZ,
			rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create output ring\n");


    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);



    /* Start rx core */
    struct lcore_params *pr =
		rte_malloc(NULL, sizeof(*pr), 0);
	if (!pr)
		rte_panic("malloc failure\n");
	*pr = (struct lcore_params){2, rx_ring,tx_ring, mbuf_pool};
	rte_eal_remote_launch((lcore_function_t *)lcore_rx,pr, 2);

    /* Start tx core */
	rte_eal_remote_launch((lcore_function_t *)lcore_tx,tx_ring, 1);

    printf("Hello world\n");

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
                   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " \n",
                   i,
                   eth_hdr->s_addr.addr_bytes[0], eth_hdr->s_addr.addr_bytes[1],
                   eth_hdr->s_addr.addr_bytes[2], eth_hdr->s_addr.addr_bytes[3],
                   eth_hdr->s_addr.addr_bytes[4], eth_hdr->s_addr.addr_bytes[5]);
            rte_pktmbuf_free(rx_pkt[i]);
        }
    }
    return 0;
}
