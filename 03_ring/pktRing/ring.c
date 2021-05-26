#include "ring.h"

int lcore_rx(struct lcore_params *p)
{
	const int socket_id = rte_socket_id();
	uint16_t port;
	struct rte_mbuf *bufs[BURST_SIZE * 2];
	printf("\nCore %u doing packet RX.\n", rte_lcore_id());

	port = 1;
	for (;;)
	{
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs,
												BURST_SIZE);
		if (unlikely(nb_rx == 0))
		{
			continue;
		}

		uint16_t nb_ret = nb_rx;
		uint16_t sent = rte_ring_sp_enqueue_bulk(p->rx_ring, (void *)bufs, nb_ret, NULL);
		if (unlikely(sent < nb_ret))
		{
			while (sent < nb_ret)
				rte_pktmbuf_free(bufs[sent++]);
		}
	}
	return 0;
}

static inline void
flush_one_port(struct output_buffer *outbuf, uint16_t queueid)
{
	unsigned int nb_tx = rte_eth_tx_burst(0, queueid,
										  outbuf->mbufs, outbuf->count);
	if (unlikely(nb_tx < outbuf->count))
	{
		do
		{
			rte_pktmbuf_free(outbuf->mbufs[nb_tx]);
		} while (++nb_tx < outbuf->count);
	}
	outbuf->count = 0;
}

static inline void
flush_all_ports(struct output_buffer *tx_buffers, uint16_t queueid)
{
	if (tx_buffers->count == 0)
		return;
	flush_one_port(tx_buffers, queueid);
}

int lcore_tx(struct tx_params *tp)
{
	const int socket_id = rte_socket_id();
	uint16_t port = 0;
	printf("\nCore %u doing packet TX.\n", rte_lcore_id());
	while (1)
	{
		struct rte_mbuf *bufs[BURST_SIZE_TX];
		const uint16_t nb_rx = rte_ring_sc_dequeue_bulk(tp->tx_ring,
														(void *)bufs, BURST_SIZE_TX, NULL);
		if (unlikely(nb_rx == 0))
		{
			//flush_all_ports(&tx_buffers, tp->queueid);
			continue;
		}
		/* for traffic we receive, queue it up for transmit */
		unsigned int nb_tx = rte_eth_tx_burst(0, tp->queueid,bufs, nb_rx);
		if (unlikely(nb_tx < nb_rx)) {
			do 
			{
				rte_pktmbuf_free(bufs[nb_tx]);

			} while(++nb_tx < nb_rx);
		}
	}
	return 0;
}
