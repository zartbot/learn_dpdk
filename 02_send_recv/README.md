## 收包程序
收包主要是采用rte_eth_rx_burst函数，首先需要从mbuf_pool中分配一些空间,采用dpdk自带的rte_pktmbuf_alloc函数
```c
    struct rte_mbuf *rx_pkt[BURST_SIZE];
    for (int i = 0; i < BURST_SIZE; i++)
    {
        rx_pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
    }
```
    
然后一个for循环，不停的接收就行了,接收时rte_eth_rx_burst(1, 0, rx_pkt, BURST_SIZE)中第一个参数为portid，第二个为队列id，由于我们这个示例每个接口只有一个队列，同时我们采用loopback cable把port0和port1对连的，因此主要就是一个port0发，port1收的场景,因此接收的portid=1. 收到报文后可以通过rte_pktbuf_mtod函数去解析报文，然后可以通过结构体内变量赋值的方式修改值，更具体的示例我们在发包函数里讲， 这里只有一个简单的parse源MAC的地址的场景
```c
    for (;;)
    {   
        
        uint16_t nb_rx = rte_eth_rx_burst(1, 0, rx_pkt, BURST_SIZE);
        if (nb_rx == 0)
        {
            continue;
        }
        struct rte_ether_hdr *eth_hdr;

        for (int i = 0; i < nb_rx; i++)
        {
            eth_hdr = rte_pktmbuf_mtod(rx_pkt[i], struct rte_ether_hdr *);
            printf("Recv Pkt[%d] from MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
                   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " \n",i,
                   eth_hdr->s_addr.addr_bytes[0], eth_hdr->s_addr.addr_bytes[1],
                   eth_hdr->s_addr.addr_bytes[2], eth_hdr->s_addr.addr_bytes[3],
                   eth_hdr->s_addr.addr_bytes[4], eth_hdr->s_addr.addr_bytes[5]);
            rte_pktmbuf_free(rx_pkt[i]);
        }
    }  
    ```
## 发包程序
本次发包程序的示例是以UDP发包为主，因此我们需要逐层初始化报文,然后报文的发送可以Burst的方式一次发送32个，我们也用这种方式来处理，报文发送的函数如下rte_eth_tx_burst.

首先我们初始化源目的MAC地址、IP地址和UDP Payload里面的内容，我们以SRoU header的一部分为例,相关的结构体定义如下：
```c
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_udp_hdr *udp_hdr;
    
    
    //Defined header in UDP
    struct SRoU
    {
        uint8_t magic_num;
        uint8_t srou_length;
        uint8_t flags;
        uint8_t next_protcol;
        uint64_t pad;
    };
```    
然后我们分别来初始化每一层
```c
    //init mac
    struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    struct rte_ether_addr d_addr = {{0x3c, 0xfd, 0xfe, 0xa9, 0xa8, 0x89}};

    //init IP header
    rte_be32_t s_ip_addr = string_to_ip("1.0.0.253");
    rte_be32_t d_ip_addr = string_to_ip("1.0.0.1");
    uint16_t ether_type = rte_cpu_to_be_16(0x0800);

    //init udp payload
    struct SRoU obj = {
        .magic_num = 1,
        .srou_length = 4,
        .flags = 0xFF,
        .next_protcol = 0,
    };
```    
初始化IP地址时有一个函数是从string转换为be32值
```c
rte_be32_t string_to_ip(char *s)
{
    unsigned char a[4];
    int rc = sscanf(s, "%hhd.%hhd.%hhd.%hhd", a + 0, a + 1, a + 2, a + 3);
    if (rc != 4)
    {
        fprintf(stderr, "bad source IP address format. Use like: 1.2.3.4\n");
        exit(1);
    }
    return (rte_be32_t)(a[3]) << 24 |
           (rte_be32_t)(a[2]) << 16 |
           (rte_be32_t)(a[1]) << 8 |
           (rte_be32_t)(a[0]);
}
```
接下来我们来从mbuf中分配空间并初始化每个报文,注意其中rte_pktmbuf_mtod_offset函数的用法，大量的报文修改都采用这种方式.
```c
    struct SRoU *msg;
    struct rte_mbuf *pkt[BURST_SIZE];

    for (int i = 0; i < BURST_SIZE; i++)
    {
        //分配空间
        pkt[i] = rte_pktmbuf_alloc(mbuf_pool);
        
        //利用rte_pktmbuf_mtod函数修改二层头，
        eth_hdr = rte_pktmbuf_mtod(pkt[i], struct rte_ether_hdr *);
        eth_hdr->d_addr = d_addr;
        
        //这里我们根据burst循环改改源MAC地址玩~
        struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, i}};
        eth_hdr->s_addr = s_addr;
        eth_hdr->ether_type = ether_type;
        
        //然后利用rte_pktmbuf_mtod_offset函数， 移到IPv4头开始的地方，并定义结构体
        ipv4_hdr = rte_pktmbuf_mtod_offset(pkt[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        ipv4_hdr->version_ihl = 0x45;
        ipv4_hdr->next_proto_id = 0x11;
        ipv4_hdr->src_addr = s_ip_addr;
        ipv4_hdr->dst_addr = d_ip_addr;
        ipv4_hdr->time_to_live = 0x40;
        

        //修改UDP头，注意大端小端转换的rte_cpu_to_be_16函数
        udp_hdr = rte_pktmbuf_mtod_offset(pkt[i], struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
        udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr));
        udp_hdr->src_port = rte_cpu_to_be_16(1234);
        udp_hdr->dst_port = rte_cpu_to_be_16(6666);
        
        msg = (struct SRoU *)(rte_pktmbuf_mtod(pkt[i], char *) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr));
        *msg = obj;
        int pkt_size = sizeof(struct SRoU) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);
        
        //最后是采用HW Offload的方式去计算Checksum
        pkt[i]->l2_len = sizeof(struct rte_ether_hdr);
        pkt[i]->l3_len = sizeof(struct rte_ipv4_hdr);
        pkt[i]->l4_len = sizeof(struct rte_udp_hdr);
        pkt[i]->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
        
        ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr) + sizeof(struct rte_ipv4_hdr));
        ipv4_hdr->hdr_checksum = 0;
        udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr, pkt[i]->ol_flags);
        
        //定义报文长度
        pkt[i]->data_len = pkt_size;
        pkt[i]->pkt_len = pkt_size;
    }
    ```
然后我们采用每两秒发送一次的方式
```c
    for(;;) {
    uint16_t nb_tx = rte_eth_tx_burst(0, 0, pkt, BURST_SIZE);
    printf("successful send %d pkts\n", nb_tx);
    sleep(2);

    }
    for (int i = 0; i < BURST_SIZE; i++)
    {
        rte_pktmbuf_free(pkt[i]);
    }
```    
执行时，我们希望收发并行执行，因此我们可以将发包函数封装好, 并在main函数中调用
```c
static int 
lcore_send(struct rte_mempool *mbuf_pool) {
...
}

int main(){

    rte_eal_remote_launch((lcore_function_t *)lcore_send,mbuf_pool,1);

}
```