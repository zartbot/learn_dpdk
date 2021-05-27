
> 前面一个示例只有单线程的收发，如果利用一个网卡进行多线程的收发呢？ 主要是网卡的TX多队列，和RX多队列，具体内容参见github上`04_rss`内的代码.

## 网卡支持情况

可以通过以下命令查看
```bash
//队列数
[zartbot@netdev multiThreadRing]$  ethtool -l ens17f0
Channel parameters for ens17f0:
Pre-set maximums:
RX:             0
TX:             0
Other:          512
Combined:       63
Current hardware settings:
RX:             0
TX:             0
Other:          0
Combined:       63

//队列深度
[zartbot@netdev multiThreadRing]$ ethtool -g ens17f0
Ring parameters for ens17f0:
Pre-set maximums:
RX:             8192
RX Mini:        0
RX Jumbo:       0
TX:             8192
Current hardware settings:
RX:             1024
RX Mini:        0
RX Jumbo:       0
TX:             1024
```

## 多个TX

多个TX Queue的配置相对容易，在`port_init`函数中初始化即可
```c
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0) 
            return retval;
    }
```
然后我们会在main函数中为每个Queue创建一个Ring
```c
   for (int i = 0; i < NUM_TX_QUEUE; ++i)
    {
        char tx_ring_name[14];
        sprintf(tx_ring_name, "output_ring%d", i);
        tx_ring[i] = rte_ring_create(tx_ring_name, SCHED_TX_RING_SZ,
                                     rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
        if (tx_ring[i] == NULL)
            rte_exit(EXIT_FAILURE, "Cannot create output ring\n");
    }
```
然后调用不同的lcore发包，这只是一个1:1的lcore_tx和lcore_send_pkt示例，通常可以让一个lcore去同时处理多个ring，并执行发包程序.
```c

struct lcore_params {
    struct rte_ring *rx_ring;
    struct rte_ring *tx_ring;
    uint16_t rx_queue_id;
    uint16_t tx_queue_id;
    struct rte_mempool *mem_pool;
};

    /* Start rx core */
    for (int i = 0; i < NUM_RX_QUEUE; ++i)
    {
        lp_rx[i] = rte_malloc(NULL, sizeof(*lp_rx[i]), 0);
        if (!lp_rx[i])
            rte_panic("malloc failure\n");
        *lp_rx[i] = (struct lcore_params){rx_ring[i], NULL, i, i, mbuf_pool};
        rte_eal_remote_launch((lcore_function_t *)lcore_rx, lp_rx[i], lcore_num++);

        //start pkt dequeue process
        rxp[i] = rte_malloc(NULL, sizeof(*rxp[i]), 0);
        if (!rxp[i])
            rte_panic("malloc failure\n");
        *rxp[i] = (struct rx_params){i,rx_ring[i]};
        
        rte_eal_remote_launch((lcore_function_t *)lcore_recv_pkt, rxp[i], lcore_num++);
    }

```
在发包的时候，由于queue_id通过lcore_params传入，因此tx函数如下
```c

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
        //发包函数带上了queue_id
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
```

## RSS

RSS是Recieve Side Scaling的意思，即通过网络数据包的一些字段构成hash值，然后分发到不同的接收队列上.网卡的支持情况如下可以查看
```bash
[zartbot@netdev multiThreadRing]$ ethtool -x ens17f0
RX flow hash indirection table for ens17f0 with 63 RX ring(s):
    0:      0     1     2     3     4     5     6     7
    8:      8     9    10    11    12    13    14    15
   16:     16    17    18    19    20    21    22    23
   24:     24    25    26    27    28    29    30    31
   32:     32    33    34    35    36    37    38    39
   40:     40    41    42    43    44    45    46    47
   48:     48    49    50    51    52    53    54    55
   56:     56    57    58    59    60    61    62     0
   64:      1     2     3     4     5     6     7     8
   72:      9    10    11    12    13    14    15    16
   80:     17    18    19    20    21    22    23    24
   88:     25    26    27    28    29    30    31    32
   96:     33    34    35    36    37    38    39    40
  104:     41    42    43    44    45    46    47    48
  112:     49    50    51    52    53    54    55    56
  120:     57    58    59    60    61    62     0     1
RSS hash key:
f4:8a:21:18:f8:03:b2:75:17:45:c5:3b:c7:db:66:b4:eb:73:ea:53:7d:13:93:3b:f1:50:65:55:e1:20:d6:3d:87:52:59:f3:f8:25:f2:2c
RSS hash function:
    toeplitz: on
    xor: off
    crc32: off
```
我们需要在port_init的阶段配置RSS
```c
static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
    .mq_mode = ETH_MQ_RX_RSS,
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP | ETH_RSS_TCP | ETH_RSS_UDP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    }
};
```
然后同样也是创建多个`rx_queue`
```c
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }
```
和上一讲区别就是在收包函数中指定了队列
```c
        const uint16_t nb_rx = rte_eth_rx_burst(port, p->rx_queue_id, bufs,
                                                BURST_SIZE);
```                                                

最终我们可以8个TX， 4个RX, 56Mpps...
```c
EAL: Detected 96 lcore(s)
EAL: Detected 2 NUMA nodes
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: No available 1048576 kB hugepages reported
EAL: Probe PCI driver: mlx5_pci (15b3:1019) device: 0000:86:00.0 (socket 1)
EAL: Probe PCI driver: mlx5_pci (15b3:1019) device: 0000:86:00.1 (socket 1)
TELEMETRY: No legacy callbacks, legacy socket not created


initializing port 0...
port[0] support RX cheksum offload.
port[0] support TX mbuf fast free offload.
port[0] support TX IPv4 checksum offload.
port[0] support TX UDP checksum offload.
port[0] support TX TCP checksum offload.
Port[0] MAC: ec:0d:9a:c5:df:fc


initializing port 1...
port[1] support RX cheksum offload.
port[1] support TX mbuf fast free offload.
port[1] support TX IPv4 checksum offload.
port[1] support TX UDP checksum offload.
port[1] support TX TCP checksum offload.
Port[1] MAC: ec:0d:9a:c5:df:fd
Core 1 doing packet RX.
Core 2 doing RX dequeue.
Core 3 doing packet RX.
Core 4 doing RX dequeue.
Core 5 doing packet RX.
Core 6 doing RX dequeue.
Core 7 doing packet RX.
Core 8 doing RX dequeue.
Core 9 doing packet TX.
Core 10 doing packet enqueue.
Core 11 doing packet TX.
Core 12 doing packet enqueue.
Core 13 doing packet TX.
Core 14 doing packet enqueue.
Core 15 doing packet TX.
Core 16 doing packet enqueue.
Core 17 doing packet TX.
Core 18 doing packet enqueue.
Core 19 doing packet TX.
Core 20 doing packet enqueue.
Core 21 doing packet TX.
Core 22 doing packet enqueue.
Core 23 doing packet TX.
Core 24 doing packet enqueue.
RX-Queue[0] PPS: 14097659
RX-Queue[1] PPS: 14100129
RX-Queue[2] PPS: 14087252
RX-Queue[3] PPS: 14089980

RX-Queue[0] PPS: 14124369
RX-Queue[1] PPS: 14124095
RX-Queue[2] PPS: 14109246
RX-Queue[3] PPS: 14109280

RX-Queue[0] PPS: 14122728
RX-Queue[1] PPS: 14122696
RX-Queue[2] PPS: 14107982
RX-Queue[3] PPS: 14107952

RX-Queue[0] PPS: 14123040
RX-Queue[1] PPS: 14122902
RX-Queue[2] PPS: 14108187
RX-Queue[3] PPS: 14108188
```
## 展望

如何有效的调度报文和如何安排多个core中的功能便是写dataplane的艺术了.当然很多时候在设计协议时，也需要考虑到不同的硬件实现，使得协议处理更加高效.
