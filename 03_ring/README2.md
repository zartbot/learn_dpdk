## 利用Ring收发报文

这个例子源文件在`02_pktRing`中,我们只创建了一个Ring,然后分别在两个不同的Core上调用收发函数
```c
   ring = rte_ring_create("message_ring", SCHED_RX_RING_SZ,
                              rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
   /* Start pkt dequeue on port 1*/
   rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, ring, lcore_num++);

   /* start pkt enqueue on port 0*/
   lp = rte_malloc(NULL, sizeof(*lp), 0);
   if (!lp)
       rte_panic("malloc failure\n");
   *lp = (struct lcore_params){ring, mbuf_pool};
   rte_eal_remote_launch((lcore_function_t *)lcore_send_pkt, lp, lcore_num ++);     
```

大概就是如下这样
```bash
lcore2(lcore_send_pkt)---->ring--->lcore1(lcore_recv_pkt)
```

发包函数如下， 主要是`rte_ring_sp_enqueue_bulk`函数
```c

int lcore_send_pkt(struct lcore_params *p)
{
    //此处省略....
    struct rte_mbuf *pkts[BURST_SIZE];

    for (;;)
    {
        //批量allocate mbuf
        rte_pktmbuf_alloc_bulk(p->mem_pool, pkts, BURST_SIZE);
        for (int i = 0; i < BURST_SIZE; i++)
        {
            //修改以太网头
            eth_hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
            eth_hdr->d_addr = d_addr;
            eth_hdr->s_addr = s_addr;
            eth_hdr->ether_type = ether_type;

            //此处省略....
        }

        //发包函数
        uint16_t nb_send = BURST_SIZE;
        uint16_t sent = rte_ring_sp_enqueue_bulk(p->ring, (void *)pkts, nb_send, NULL);

        //没有发出去的包需要Free掉
        if (unlikely(sent < nb_send))
        {
            while (sent < nb_send)
            {
                rte_pktmbuf_free(pkts[sent++]);
            }
        }
    }
    return 0;
}
```

收包函数和以前一样,计算pps.

## 单线程以太网收发包

在前一个示例的基础上，我们改成如下这样，代码在`03_singleThreadPktRing`内
```bash
lcore_send_pkt-->tx_ring-->lcore_tx--Port0-->Port1-->lcore_rx-->lcore_recv_pkt
```
主要是增加了两个额外的函数,接收时，从`rte_eth_rx_burst`收包，然后`rte_ring_sp_enqueue_bulk`
```c

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
```
发送时，从`rte_ring_sc_dequeue_bulk`收包，然后`rte_eth_tx_burst`网卡发出去
```c

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
```




