#include "ring.h"

int lcore_rx(struct lcore_params *p)
{
     uint16_t port;
    struct rte_mbuf *bufs[BURST_SIZE * 2];
    printf("Core %u doing packet RX.\n", rte_lcore_id());

    port = 1;
    for (;;)
    {
        const uint16_t nb_rx = rte_eth_rx_burst(port, p->rx_queue_id, bufs,
                                                BURST_SIZE);
        if (unlikely(nb_rx == 0))
        {
            continue;
        }
        uint16_t nb_ret = nb_rx;
        uint16_t sent = rte_ring_enqueue_burst(p->rx_ring, (void *)bufs, nb_ret, NULL);
        if (unlikely(sent < nb_ret))
        {
            while (sent < nb_ret)
                rte_pktmbuf_free(bufs[sent++]);
        }
    }
    return 0;
}

int lcore_tx(struct lcore_params *p)
{
    uint16_t port = 0;
    printf("Core %u doing packet TX.\n", rte_lcore_id());
    struct rte_mbuf *bufs[BURST_SIZE_TX];
    while (1)
    {        
        const uint16_t nb_rx = rte_ring_dequeue_burst(p->tx_ring,
                                                        (void *)bufs, BURST_SIZE_TX, NULL);
        if (unlikely(nb_rx == 0))
        {
            
            continue;
        }
        unsigned int nb_tx = rte_eth_tx_burst(port, p->tx_queue_id, bufs, nb_rx);
        if (unlikely(nb_tx < nb_rx)) {
            do 
            {
                rte_pktmbuf_free(bufs[nb_tx]);
            } while(++nb_tx < nb_rx);
        }
    }
    return 0;
}
