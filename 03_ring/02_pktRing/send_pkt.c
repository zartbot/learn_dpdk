
#include "send_pkt.h"

rte_be32_t string_to_ip(char *s)
{
    unsigned char a[4];
    int rc = sscanf(s, "%hhd.%hhd.%hhd.%hhd", a + 0, a + 1, a + 2, a + 3);
    if (rc != 4)
    {
        fprintf(stderr, "bad source IP address format. Use like: 1.1.1.1\n");
        exit(1);
    }
    return (rte_be32_t)(a[3]) << 24 |
           (rte_be32_t)(a[2]) << 16 |
           (rte_be32_t)(a[1]) << 8 |
           (rte_be32_t)(a[0]);
}

int lcore_send_pkt(struct lcore_params *p)
{
    const int socket_id = rte_socket_id();
    printf("\nCore %u doing packet enqueue.\n", rte_lcore_id());

    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_udp_hdr *udp_hdr;

    //init mac
    struct rte_ether_addr s_addr = {{0xec, 0x0d, 0x9a, 0xc5, 0xdf, 0xfc}};
    struct rte_ether_addr d_addr = {{0xec, 0x0d, 0x9a, 0xc5, 0xdf, 0xfd}};

    //init IP header
    rte_be32_t s_ip_addr = string_to_ip("1.0.0.253");
    rte_be32_t d_ip_addr = string_to_ip("1.0.0.1");
    uint16_t ether_type = rte_cpu_to_be_16(0x0800);
    //Defined header in UDP
    struct SRoU
    {
        uint8_t magic_num;
        uint8_t srou_length;
        uint8_t flags;
        uint8_t next_protcol;
        uint64_t pad;
    };

    struct rte_mbuf *pkts[BURST_SIZE];

    for (;;)
    {
        rte_pktmbuf_alloc_bulk(p->mem_pool, pkts, BURST_SIZE);

        for (int i = 0; i < BURST_SIZE; i++)
        {
            //pkts[i] = rte_pktmbuf_alloc(p->mem_pool);
            //build_pkt(pkts[i]);

            eth_hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
            eth_hdr->d_addr = d_addr;
            eth_hdr->s_addr = s_addr;
            eth_hdr->ether_type = ether_type;

            ipv4_hdr = rte_pktmbuf_mtod_offset(pkts[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
            ipv4_hdr->version_ihl = 0x45;
            ipv4_hdr->next_proto_id = 0x11;
            ipv4_hdr->src_addr = s_ip_addr;
            ipv4_hdr->dst_addr = d_ip_addr;
            ipv4_hdr->time_to_live = 0x40;

            udp_hdr = rte_pktmbuf_mtod_offset(pkts[i], struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
            udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr));
            udp_hdr->src_port = rte_cpu_to_be_16(1234);
            udp_hdr->dst_port = rte_cpu_to_be_16(6666);
            ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct SRoU) + sizeof(struct rte_udp_hdr) + sizeof(struct rte_ipv4_hdr));

            //init udp payload
            struct SRoU obj = {
                .magic_num = 1,
                .srou_length = 4,
                .flags = 0xFF,
                .next_protcol = 0,
            };
            struct SRoU *msg;

            msg = (struct SRoU *)(rte_pktmbuf_mtod(pkts[i], char *) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr));
            *msg = obj;

            int pkt_size = sizeof(struct SRoU) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);

            pkts[i]->l2_len = sizeof(struct rte_ether_hdr);
            pkts[i]->l3_len = sizeof(struct rte_ipv4_hdr);
            pkts[i]->l4_len = sizeof(struct rte_udp_hdr);
            pkts[i]->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
            ipv4_hdr->hdr_checksum = 0;
            udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr, pkts[i]->ol_flags);
            pkts[i]->data_len = pkt_size;
            pkts[i]->pkt_len = pkt_size;
        }

        uint16_t nb_send = BURST_SIZE;
        uint16_t sent = rte_ring_sp_enqueue_bulk(p->ring, (void *)pkts, nb_send, NULL);

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
