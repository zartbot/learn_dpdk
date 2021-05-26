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
    struct rte_ring *rx_ring;
    struct tx_params *tp[NUM_TX_QUEUE];
    struct lcore_params *lp[NUM_TX_QUEUE];
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

    rx_ring = rte_ring_create("Input_ring", SCHED_RX_RING_SZ,
                              rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
    if (rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create output ring\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);

    unsigned int lcore_num = 37;
    /* Start tx core */
    for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        tp[i] = rte_malloc(NULL, sizeof(*tp[i]), 0);
        if (!tp[i])
            rte_panic("malloc failure\n");
        *tp[i] = (struct tx_params){i, tx_ring[i]};
        rte_eal_remote_launch((lcore_function_t *)lcore_tx, tp[i], lcore_num++);
    }

    /* Start rx core */
    struct lcore_params *pr = rte_malloc(NULL, sizeof(*pr), 0);
    if (!pr)
        rte_panic("malloc failure\n");
    *pr = (struct lcore_params){5, rx_ring, tx_ring[0], mbuf_pool};
    rte_eal_remote_launch((lcore_function_t *)lcore_rx, pr, lcore_num++);

    /* Start pkt dequeue on port 1*/
    rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, rx_ring, lcore_num++);

    /* start pkt enqueue on port 0*/

    for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        lp[i] = rte_malloc(NULL, sizeof(*lp[i]), 0);
        if (!lp[i])
            rte_panic("malloc failure\n");
        *lp[i] = (struct lcore_params){i, rx_ring, tx_ring[i], mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp[i], lcore_num ++);
    }

    

    rte_eal_wait_lcore(lcore_num-1);
    return 0;
}
