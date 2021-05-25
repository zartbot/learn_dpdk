#include <rte_ethdev.h>
#include "tx_ring.h"


struct output_buffer {
	unsigned count;
	struct rte_mbuf *mbufs[BURST_SIZE];
};


static inline void
flush_one_port(struct output_buffer *outbuf, uint8_t outp)
{
	unsigned int nb_tx = rte_eth_tx_burst(outp, 0,
			outbuf->mbufs, outbuf->count);

	if (unlikely(nb_tx < outbuf->count)) {
		do {
			rte_pktmbuf_free(outbuf->mbufs[nb_tx]);
		} while (++nb_tx < outbuf->count);
	}
	outbuf->count = 0;
}

static inline void
flush_all_ports(struct output_buffer *tx_buffers)
{
	uint16_t outp;

	RTE_ETH_FOREACH_DEV(outp) {
		if (tx_buffers[outp].count == 0)
			continue;
		flush_one_port(&tx_buffers[outp], outp);
	}
}

int
lcore_tx(struct rte_ring *in_r)
{
	static struct output_buffer tx_buffers[RTE_MAX_ETHPORTS];
	const int socket_id = rte_socket_id();
	uint16_t port=0;

	printf("\nCore %u doing packet TX.\n", rte_lcore_id());
	while (1) {
			struct rte_mbuf *bufs[BURST_SIZE_TX];
			const uint16_t nb_rx = rte_ring_dequeue_burst(in_r,
					(void *)bufs, BURST_SIZE_TX, NULL);
			
			/* if we get no traffic, flush anything we have */
			if (unlikely(nb_rx == 0)) {
				flush_all_ports(tx_buffers);
				continue;
			}

			/* for traffic we receive, queue it up for transmit */
			uint16_t i;
			rte_prefetch_non_temporal((void *)bufs[0]);
			rte_prefetch_non_temporal((void *)bufs[1]);
			rte_prefetch_non_temporal((void *)bufs[2]);
			for (i = 0; i < nb_rx; i++) {
				struct output_buffer *outbuf;
				uint8_t outp;
				rte_prefetch_non_temporal((void *)bufs[i + 3]);
				outp = bufs[i]->port;

				outbuf = &tx_buffers[outp];
				outbuf->mbufs[outbuf->count++] = bufs[i];
				if (outbuf->count == BURST_SIZE_TX)
					flush_one_port(outbuf, outp);
			}
	}
	return 0;
}
