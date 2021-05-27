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

int lcore_tx(struct rte_ring *ring)
{
    const int socket_id = rte_socket_id();
    uint16_t port = 0;
    printf("\nCore %u doing packet TX.\n", rte_lcore_id());
    while (1)
    {
        struct rte_mbuf *bufs[BURST_SIZE_TX];
        const uint16_t nb_rx = rte_ring_sc_dequeue_bulk(ring,
                                                        (void *)bufs, BURST_SIZE_TX, NULL);
        if (unlikely(nb_rx == 0))
            continue;

        /* for traffic we receive, queue it up for transmit */
        unsigned int nb_tx = rte_eth_tx_burst(0, 0,bufs, nb_rx);
        if (unlikely(nb_tx < nb_rx)) {
            do 
            {
                rte_pktmbuf_free(bufs[nb_tx]);

            } while(++nb_tx < nb_rx);
        }
    }
    return 0;
}
