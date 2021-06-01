#include "recv_pkt.h"

int lcore_recv_pkt(struct lcore_params *p)
{
    const int socket_id = rte_socket_id();
    printf("Core %u doing RX dequeue.\n", rte_lcore_id());
    uint64_t freq = rte_get_tsc_hz();

    uint64_t pkt_cnt = 0;
    uint64_t now = rte_rdtsc();

    while (1)
    {
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(0, p->rx_queue_id, bufs,
                                                BURST_SIZE);
        
        if (unlikely(rte_rdtsc() - now > freq))
        {
            printf("RX-Queue[%d] PPS: %ld\n",p->rx_queue_id, pkt_cnt);
            pkt_cnt = 0;
            now = rte_rdtsc();
        }
        if (unlikely(nb_rx == 0))
        {
            continue;
        }
        pkt_cnt += nb_rx;

        uint16_t i;
        for (i = 0; i < nb_rx; i++)
        {
            rte_pktmbuf_free(bufs[i]);
        }

    }
    return 0;
}
