## 配置端口

首先需要初始化EAL
```c
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
      rte_exit(EXIT_FAILURE, "initlize fail!");
```

然后通过`rte_eth_dev_count_avail()`函数获取系统的接口数目:
```c
  int nb_ports;
  nb_ports = rte_eth_dev_count_avail();
  printf("number of available port: %d\n", nb_ports);
```
然后我们可以通过rte_eth_dev_info_get如下方式获取device info：
```c
    struct rte_eth_dev_info dev_info;
    for (int portid = 0; portid < nb_ports; ++portid)
    {
        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get device info: err=%d, port=%d\n", ret, portid);
        printf("port: %d Driver:%s\n", portid, dev_info.driver_name);
    }
```
最后可以根据rte_eth_macaddr_get函数获取接口MAC地址,并放在ports_eth_addr数组中.
```c
    static struct rte_ether_addr ports_eth_addr[MAX_PORTS];
    for (int portid = 0; portid < nb_ports; ++portid)
    {
        ret = rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot get MAC address: err=%d, port=%d\n", ret, portid);

        char mac[18];
        rte_ether_format_addr(&mac[0], 18, &ports_eth_addr[portid]);
        printf("port: %d->MAC-> %s\n", portid, mac);
    }
```

## 复杂一点的接口初始化

在需要用DPDK收发包时，通常我们需要做更复杂的接口初始化操作，因此我们通常会专门写一个port_init函数,这个函数的参数为portid和相关的mbuf_pool：

```c
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
...
}

```

首先我们需要定义一些常量， 主要是RX_RING / TX_RING的大小， MBUF的大小和Cache_size等

```c
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
```

接着定义一个default config的结构体
```c
static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
    },
};
```
接下来就是整个port_init函数了：

```c
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;
    
    //查看这个接口是否为valid，非法则返回-1
    if (!rte_eth_dev_is_valid_port(port))
        return -1;
        
    //获取接口信息
    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0)
    {
        printf("Error during getting device (port %u) info: %s\n",
               port, strerror(-retval));
        return retval;
    }
    printf("\n\ninitializing port %d...\n",port);

    //查看接口硬件Offload的能力是否支持，如果支持打开该功能
    if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_CHECKSUM)
    {
        printf("port[%u] support RX cheksum offload.\n", port);
        port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_CHECKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    {
        printf("port[%u] support TX mbuf fast free offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM)
    {
        printf("port[%u] support TX IPv4 checksum offload.\n", port);

        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
    }

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM)
    {
        printf("port[%u] support TX UDP checksum offload.\n", port);
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;
    }

    //配置接口
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    //分配RX队列
    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    
    //分配TX队列
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    //使能接口
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    //获取接口MAC地址
    struct rte_ether_addr addr;
    retval = rte_eth_macaddr_get(port, &addr);
    if (retval != 0)
        return retval;

    printf("Port[%u] MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
           " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
           port,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    //打开混杂模式
    retval = rte_eth_promiscuous_enable(port);
    if (retval != 0)
        return retval;

    return 0;
}
```
然后main函数就很简单的了, 初始化EAL，然后创建Mbuf pool
```c
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
```        
然后初始化接口时，可以使用一个宏RTE_ETH_FOREACH_DEV(portid)将每个接口使能.

```
int main(int argc, char *argv[])
{

    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint16_t portid;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "initlize fail!");

    printf("\n\n\n*****************************************\n");

    nb_ports = rte_eth_dev_count_avail();
    printf("number of available port: %d\n", nb_ports);

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);

    return 0;
}
```



