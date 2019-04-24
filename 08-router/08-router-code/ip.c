#include "ip.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	struct icmphdr * icmp_hdr = (struct icmphdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (daddr == iface->ip && icmp_hdr->type == ICMP_ECHOREQUEST) {
		printf("handle ip packet: echo request.\n");
		icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		free(packet);
	}
	else {
		ip_forward_packet(daddr, packet, len);
	}
}
