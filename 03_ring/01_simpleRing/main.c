
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

static const char *_MSG_POOL = "MSG_POOL";
static const char *_Z_RING = "ZARTBOT_RING";

struct rte_ring *send_ring, *zartbot_ring;
struct rte_mempool *message_pool;
volatile int quit = 0;

static int
lcore_recv(__rte_unused void *arg)
{
    unsigned lcore_id = rte_lcore_id();
    printf("Starting core %u\n", lcore_id);
    while (!quit){
        void *msg;
        if (rte_ring_dequeue(zartbot_ring, &msg) < 0){
            usleep(5);
            continue;
        }
        printf("core %u: Received '%s'\n", lcore_id, (char *)msg);
        rte_mempool_put(message_pool, msg);
    }
    return 0;
}

static int
lcore_send(__rte_unused void *arg)
{
unsigned lcore_id = rte_lcore_id();
struct rte_ring *tx_ring;
    printf("Starting core %u\n", lcore_id);
    void *msg = NULL;
    if (rte_mempool_get(message_pool, &msg) < 0)
        rte_panic("Failed to get message buffer\n");
    
    for (int i=0;i<100;i++) {
        strlcpy((char *)msg, "hello", 128);
        if (rte_ring_enqueue(zartbot_ring, msg) < 0) {
            printf("Failed to send message - message discarded\n");
            rte_mempool_put(message_pool, msg);
        }
        sleep(1);
    }
}

int
main(int argc, char **argv)
{
    const unsigned flags = 0;
    const unsigned ring_size = 1024*32;
    const unsigned pool_size = 1023;
    const unsigned pool_cache = 32;
    const unsigned priv_data_sz = 0;

    int ret;
    unsigned lcore_id;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot init EAL\n");

    zartbot_ring = rte_ring_create(_Z_RING, ring_size, rte_socket_id(), flags);
    message_pool = rte_mempool_create(_MSG_POOL, pool_size,
                128, pool_cache, priv_data_sz,
                NULL, NULL, NULL, NULL,
                rte_socket_id(), flags);

    if (zartbot_ring == NULL)
        rte_exit(EXIT_FAILURE, "Problem getting receiving ring\n");
    if (message_pool == NULL)
        rte_exit(EXIT_FAILURE, "Problem getting message pool\n");

    RTE_LOG(INFO, APP, "Finished Process Init.\n");

    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (lcore_id == 1) {
            rte_eal_remote_launch(lcore_send, NULL, lcore_id);
        } 
        if (lcore_id == 2) {
            rte_eal_remote_launch(lcore_recv, NULL, lcore_id);
        }
        
    }
    
    rte_eal_mp_wait_lcore();
    return 0;
}
