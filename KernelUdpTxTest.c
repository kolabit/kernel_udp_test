////////////////////////////////////////////////////////////////////////////////
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//! @file: KernelUdpTxTest.c
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//! @brief: Linux Kernel UDP Socket Tx test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Created: 31-JAN-2024
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
////////////////////////////////////////////////////////////////////////////////


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/sock.h>

////////////////////////////////////////////////////////////////////////////////
// SET YOUR CORRECT NETWORK DATA BELOW
//
// Network interface on source Linux PC
#define IF_NAME "eth1"

// Source Interface info
//#define SRC_MAC {0x6c, 0xb3, 0x11, 0x52, 0x77, 0xAA}
#define SRC_MAC {0x0,0x04,0xa3,0x33,0x6a,0xce}
#define SRC_IP  "10.22.33.16"
#define SRC_PORT 0xC000

// Destination Interface info
#define DST_MAC {0x24, 0x4B, 0xFE, 0x5A, 0x22, 0xFF}
#define DST_IP   "10.22.33.1"
#define DST_PORT 0xD456

////////////////////////////////////////////////////////////////////////////////

// Number of test frames
#define TEST_FRM_NUM 10000
// Size of the test packet(s)
#define TEST_DATA_LEN 3080



struct socket *g_sock;

static void udp_tx_by_skb(void)
{
        struct net_device *netdev = NULL;
        struct net *net = NULL;
        struct sk_buff *skb = NULL;
        struct ethhdr *eth_header = NULL;
        struct iphdr *ip_header = NULL;
        struct udphdr *udp_header = NULL;
        __be32 dip = in_aton(DST_IP);
        __be32 sip = in_aton(SRC_IP);
        //u8 buff[TEST_DATA_LEN] = {};
        u8 *buff = NULL;
        int i=0;
        u16 data_len = TEST_DATA_LEN;
        u8 *pdata = NULL;
        u32 skb_len;
        u8 dst_mac[ETH_ALEN] = DST_MAC;
        u8 src_mac[ETH_ALEN] = SRC_MAC;
        unsigned int transp_offset=0;

        // Allocate the test data buffer
        buff  = kmalloc(TEST_DATA_LEN, GFP_KERNEL);
        if(!buff)
        {
                printk("KernelUdpTxTest: ERROR!!! kmalloc failed!\n");
                return; 
        }
        // Fill the test data buffer
        for(i=0;i<TEST_DATA_LEN;i++)
        {
             buff[i] = (u8)(i&0xFF);   
        }


        // Create kernel socket
        sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, 0, &g_sock);

        //
        net = sock_net((const struct sock *) g_sock->sk);
        netdev = dev_get_by_name(net, IF_NAME);

        for(i=0;i<TEST_FRM_NUM;i++)
        {
                //printk("KernelUdpTxTest: UDP TX %i\n", i);
            
                // LL_RESERVED_SPACE is defined in include/netdevice.h. */
                skb_len = data_len + sizeof(struct iphdr) + sizeof(struct udphdr) + LL_RESERVED_SPACE(netdev);
                //printk("KernelUdpTxTest: data_len: %i, skb_len: %i\n", data_len, skb_len);

                // Allocate the
                skb = dev_alloc_skb(skb_len);
                if (!skb) 
                {
                        printk("KernelUdpTxTest: ERROR!!! dev_alloc_skb failed!\n");
                        kfree(buff);
                        return;
                }


                // Reserve the space for protocol headers.
                // PACKET_OTHERHOST: packet type is "to someone else".
                // ETH_P_IP: Internet Protocol packet.
                // CHECKSUM_NONE means checksum calculation is done by software.
                skb_reserve(skb, LL_RESERVED_SPACE(netdev));
                skb->dev = netdev;
                skb->pkt_type = PACKET_OTHERHOST;
                skb->protocol = htons(ETH_P_IP);
                skb->ip_summed = CHECKSUM_NONE;
                skb->priority = 0;

                // Allocate memory for ip header
                skb_set_network_header(skb, 0);
                skb_put(skb, sizeof(struct iphdr));

                // Allocate memory for udp header
                skb_set_transport_header(skb, sizeof(struct iphdr));
                skb_put(skb, sizeof(struct udphdr));
                
                // Fill UDP Header
                udp_header = udp_hdr(skb);
                udp_header->source = htons(SRC_PORT);
                udp_header->dest = htons(DST_PORT);
                udp_header->check = 0;
                udp_header->len = htons(data_len+8);
        
                // Fille IP Header
                ip_header = ip_hdr(skb);
                ip_header->version = 4;
                ip_header->ihl = sizeof(struct iphdr) >> 2;
                ip_header->frag_off = htons(0x4000);
                ip_header->protocol = IPPROTO_UDP;
                ip_header->tos = 0;
                ip_header->daddr = dip;
                ip_header->saddr = sip;
                ip_header->ttl = 0x40;
                ip_header->tot_len = htons(28+data_len);
                ip_header->check = 0;
                
                // Put the data to SKB
                pdata = skb_put(skb, data_len);
                if (pdata) 
                {
                        memcpy(pdata, buff, data_len);
                }
                else
                {
                        printk("KernelUdpTxTest: ERROR!!! skb_put failed!\n");
                        dev_put(netdev);
                        kfree_skb(skb);
                        kfree(buff);
                        return;
                }

                // Caculate the Checksum
                transp_offset = skb_transport_offset(skb);
                skb->csum = skb_checksum(skb, transp_offset, skb->len-transp_offset, 0);
                ip_header->check = ip_fast_csum(ip_header, ip_header->ihl);
                udp_header->check = csum_tcpudp_magic(sip, dip, skb->len-transp_offset, IPPROTO_UDP, skb->csum);
        
                // Build the Ethernet header in SKB
                eth_header = ( struct ethhdr *)skb_push(skb, ETH_HLEN);
                memcpy(eth_header->h_dest, dst_mac, ETH_ALEN);
                memcpy(eth_header->h_source, src_mac, ETH_ALEN);
                eth_header->h_proto = htons(ETH_P_IP);       

                // Send the packet
                if (dev_queue_xmit(skb) < 0) 
                {
                        dev_put(netdev);
                        kfree_skb(skb);
                        kfree(buff);
                        printk("KernelUdpTxTest: ERROR!!! dev_queue_xmit failed!\n");
                        return;
                }
       
                //!!!kfree_skb(skb); You should NOT release the SKB
        }

        // Free the data buffer
        kfree(buff);

        // Free the socket
        sock_release(g_sock);

        // ALL DONE
        printk("KernelUdpTxTest: UDP TX OK\n");
}

static int __init KernelUdpTxTest_init(void)
{
        printk("KernelUdpTxTest_init\n");
        udp_tx_by_skb();
        return 0;
}

static void __exit KernelUdpTxTest_exit(void)
{
        printk("KernelUdpTxTest_exit\n");
}

module_init(KernelUdpTxTest_init);
module_exit(KernelUdpTxTest_exit);

MODULE_LICENSE("GPL");
