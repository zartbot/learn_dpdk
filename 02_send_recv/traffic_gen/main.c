#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include "portinit.h"
#include "send_pkt.h"
#include "recv_pkt.h"

int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    struct rte_ring *tx_ring[NUM_TX_QUEUE];
    struct rte_ring *rx_ring[NUM_RX_QUEUE];
    struct lcore_params *lp_tx[NUM_TX_QUEUE];
    struct lcore_params *lp_rx[NUM_RX_QUEUE];
    struct rx_params *rxp[NUM_RX_QUEUE];
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    if (port_init(0, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port 0\n");

    unsigned int lcore_num = 1;

    /* Start rx core */
    for (int i = 0; i < NUM_RX_QUEUE; ++i)
    {
        lp_rx[i] = rte_malloc(NULL, sizeof(*lp_rx[i]), 0);
        if (!lp_rx[i])
            rte_panic("malloc failure\n");
        *lp_rx[i] = (struct lcore_params){i, i, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, lp_rx[i], lcore_num++);
    }

    
    /* Start tx core */
    for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        lp_tx[i] = rte_malloc(NULL, sizeof(*lp_tx[i]), 0);
        if (!lp_tx[i])
            rte_panic("malloc failure\n");
        *lp_tx[i] = (struct lcore_params){i, i, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp_tx[i], lcore_num++);
    }


    rte_eal_wait_lcore(lcore_num - 1);
    return 0;
}
