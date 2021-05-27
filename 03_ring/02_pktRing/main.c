#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include "send_pkt.h"
#include "recv_pkt.h"

int main(int argc, char *argv[])
{

    struct rte_mempool *mbuf_pool;
    struct rte_ring *ring;
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

    ring = rte_ring_create("message_ring", SCHED_RX_RING_SZ,
                              rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
    if (ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create ring\n");

    unsigned int lcore_num = 1;
 
     /* Start pkt dequeue on port 1*/
    rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, ring, lcore_num++);

    /* start pkt enqueue on port 0*/
     lp = rte_malloc(NULL, sizeof(*lp), 0);
        if (!lp)
            rte_panic("malloc failure\n");
        *lp = (struct lcore_params){ring, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp, lcore_num ++);

    rte_eal_wait_lcore(lcore_num-1);
    return 0;
}
