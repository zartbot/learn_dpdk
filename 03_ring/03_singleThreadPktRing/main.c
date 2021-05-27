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
    struct rte_ring *tx_ring;
    struct rte_ring *rx_ring;
    struct lcore_params *lp;
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

    tx_ring = rte_ring_create("output_ring", SCHED_TX_RING_SZ,
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

    unsigned int lcore_num = 1;
    /* Start tx core */
    rte_eal_remote_launch((lcore_function_t *)lcore_tx, tx_ring, lcore_num++);

    /* Start rx core */
    struct lcore_params *pr = rte_malloc(NULL, sizeof(*pr), 0);
    if (!pr)
        rte_panic("malloc failure\n");
    *pr = (struct lcore_params){rx_ring, tx_ring, mbuf_pool};
    rte_eal_remote_launch((lcore_function_t *)lcore_rx, pr, lcore_num++);

    /* Start pkt dequeue on port 1*/
    rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, rx_ring, lcore_num++);

    /* start pkt enqueue on port 0*/
    lp = rte_malloc(NULL, sizeof(*lp), 0);
    if (!lp)
        rte_panic("malloc failure\n");
    *lp = (struct lcore_params){rx_ring, tx_ring, mbuf_pool};
    rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp, lcore_num++);

    rte_eal_wait_lcore(lcore_num - 1);
    return 0;
}
