#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include "portinit.h"
#include "send_pkt.h"
#include "recv_pkt.h"
#include "ring.h"

int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    struct rte_ring *tx_ring[NUM_TX_QUEUE];
    struct rte_ring *rx_ring[NUM_RX_QUEUE];
    struct lcore_params *lp_tx[NUM_TX_QUEUE];
    struct lcore_params *lp_rx[NUM_RX_QUEUE];
    struct rx_params *rxp[NUM_RX_QUEUE];
    unsigned nb_ports = 2;
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        char tx_ring_name[14];
        sprintf(tx_ring_name, "output_ring%d", i);
        tx_ring[i] = rte_ring_create(tx_ring_name, SCHED_TX_RING_SZ,
                                     rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
        if (tx_ring[i] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create output ring\n");
    }

    for (int i = 0; i < NUM_RX_QUEUE; ++i)
    {
        char rx_ring_name[14];
        sprintf(rx_ring_name, "input_ring%d", i);
        rx_ring[i] = rte_ring_create(rx_ring_name, SCHED_RX_RING_SZ,
                                     rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
        if (rx_ring[i] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create input ring\n");
    }

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);

    unsigned int lcore_num = 1;

    /* Start rx core */
    for (int i = 0; i < NUM_RX_QUEUE; ++i)
    {
        lp_rx[i] = rte_malloc(NULL, sizeof(*lp_rx[i]), 0);
        if (!lp_rx[i])
            rte_panic("malloc failure\n");
        *lp_rx[i] = (struct lcore_params){rx_ring[i], NULL, i, i, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_rx, lp_rx[i], lcore_num++);
        //start pkt dequeue process
        rxp[i] = rte_malloc(NULL, sizeof(*rxp[i]), 0);
        if (!rxp[i])
            rte_panic("malloc failure\n");
        *rxp[i] = (struct rx_params){i,rx_ring[i]};
        rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, rxp[i], lcore_num++);
    }

    
    /* Start tx core */
    for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        lp_tx[i] = rte_malloc(NULL, sizeof(*lp_tx[i]), 0);
        if (!lp_tx[i])
            rte_panic("malloc failure\n");
        *lp_tx[i] = (struct lcore_params){NULL, tx_ring[i], i, i, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_tx, lp_tx[i], lcore_num++);
        //start packet enqueue
        rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp_tx[i], lcore_num++);
    }


    rte_eal_wait_lcore(lcore_num - 1);
    return 0;
}
