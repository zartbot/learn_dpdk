#include "rx_ring.h"


int
lcore_rx(struct lcore_params *p)
{
	const uint16_t nb_ports = rte_eth_dev_count_avail();
	const int socket_id = rte_socket_id();
	uint16_t port;
	struct rte_mbuf *bufs[BURST_SIZE*2];
	printf("\nCore %u doing packet RX.\n", rte_lcore_id());

	port = 1;
	for(;;) {
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs,
				BURST_SIZE);
		if (unlikely(nb_rx == 0)) {
			if (++port == nb_ports)
				port = 0;
			continue;
		}

		uint16_t nb_ret = nb_rx;
		struct rte_ring *out_ring = p->rx_ring;

		uint16_t sent = rte_ring_enqueue_burst(out_ring,
				(void *)bufs, nb_ret, NULL);

		if (unlikely(sent < nb_ret)) {
			while (sent < nb_ret)
				rte_pktmbuf_free(bufs[sent++]);
		}
		if (++port == nb_ports)
			port = 0;
	}

	return 0;
}