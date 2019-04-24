#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	fprintf(stderr, "Send icmp packet.\n");
	
	struct iphdr *ip = packet_to_ip_hdr(in_pkt);
	u32 daddr = ntohl(ip->saddr);
	u32 saddr = longest_prefix_match(daddr)->iface->ip;

	long tot_size = (type == ICMP_ECHOREPLY) ? len - IP_HDR_SIZE(ip) + IP_BASE_HDR_SIZE : 
					IP_BASE_HDR_SIZE + ETHER_HDR_SIZE + ICMP_HDR_SIZE + IP_HDR_SIZE(ip) + 8;
	
	char *pkt = (char *)malloc(tot_size);
	
	struct icmphdr *icmp = (struct icmphdr *)(pkt + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);

	struct iphdr *ip_hdr = packet_to_ip_hdr(pkt);
	ip_init_hdr(ip_hdr, saddr, daddr, tot_size-ETHER_HDR_SIZE, IPPROTO_ICMP);

	struct ether_header *eh = (struct ether_header *)pkt;
	eh->ether_type = htons(ETH_P_IP);

	memset(icmp, 0, ICMP_HDR_SIZE);
	icmp->code = code;
	icmp->type = type;

	if (type == ICMP_ECHOREPLY) 
		memcpy((char *)icmp+ICMP_HDR_SIZE-4, in_pkt+ETHER_HDR_SIZE+IP_HDR_SIZE(ip)+4, len-ETHER_HDR_SIZE-IP_HDR_SIZE(ip)-4);
	else
		memcpy((char *)icmp+ICMP_HDR_SIZE, (char *)ip, IP_HDR_SIZE(ip)+8);
	
	icmp->checksum = icmp_checksum(icmp, tot_size-ETHER_HDR_SIZE-IP_BASE_HDR_SIZE);

    ip_send_packet(pkt, tot_size);

}
