#ifndef __RING_COMMON_H_
#define __RING_COMMON_H_

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS ((8*1024)-1)
#define MBUF_CACHE_SIZE 128
#define SCHED_RX_RING_SZ 8192
#define SCHED_TX_RING_SZ 65536

#define BURST_SIZE 64
#define BURST_SIZE_TX 32

struct lcore_params {
	unsigned worker_id;
	struct rte_ring *rx_ring;
	struct rte_ring *tx_ring;
	struct rte_mempool *mem_pool;
};

#endif