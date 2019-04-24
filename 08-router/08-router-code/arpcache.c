#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "packet.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	fprintf(stderr, "Lookup ip address in arp cache.\n");
	pthread_mutex_lock(&arpcache.lock);
	int found = 0;
	for (int i = 0; i < MAX_ARP_SIZE; i ++) {
		if (arpcache.entries[i].ip4 == ip4) {
			memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
			found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
	return found;
}

// append the packet to arpcache
//
// Lookup in the list which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	fprintf(stderr, "Append ip address.\n");

	struct arp_req *req_entry = NULL, *found_req = NULL, *q_req;
	pthread_mutex_lock(&arpcache.lock);
	struct cached_pkt *append_pkt = (struct cached_pkt *)malloc(sizeof(struct cached_pkt));
	init_list_head(&append_pkt->list);
	append_pkt->packet = packet;
	append_pkt->len = len;

	list_for_each_entry_safe(req_entry, q_req, &(arpcache.req_list), list) {
		if (found_req == NULL && req_entry->ip4 == ip4) {
			found_req = req_entry;
			break;
		}
	}

	if (!found_req) {
		req_entry = (struct arp_req *)malloc(sizeof(struct arp_req));
		init_list_head(&req_entry->list);
		req_entry->ip4 = ip4;
		req_entry->retries = 0;
		req_entry->sent = time(NULL);
		req_entry->iface = (iface_info_t *)malloc(sizeof(iface_info_t));
    	memcpy(req_entry->iface, iface, sizeof(iface_info_t));
		
		init_list_head(&req_entry->cached_packets);
		list_add_tail(&req_entry->list, &arpcache.req_list);
	}

	// insert packet to the IP
	list_add_tail(&append_pkt->list, &req_entry->cached_packets);

	// send arp request
	arp_send_request(iface, ip4);
	req_entry->retries += 1;
	req_entry->sent = time(NULL);
	
	pthread_mutex_unlock(&arpcache.lock);

}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	fprintf(stderr, "Insert ip->mac entry, and send all pending packets.\n");
	struct arp_req *req_entry, *q_req;
	struct cached_pkt *pkt_entry, *q_pkt;

	pthread_mutex_lock(&arpcache.lock);
	// find a free entry
	int i = 0;
	for ( ; i < MAX_ARP_SIZE; i ++) {
		if (!arpcache.entries[i].valid)
			break;
	}
	int free_id = (i == MAX_ARP_SIZE) ? (rand() % MAX_ARP_SIZE) : i;

	// lookup arp chache seq corresponding to the IP
	int found = 0;
	list_for_each_entry_safe(req_entry, q_req, &(arpcache.req_list), list) {
		if (req_entry->ip4 == ip4) {
			found = 1;
			break;
		}
	}
	if (found) {
		arpcache.entries[free_id].ip4 = ip4;
		arpcache.entries[free_id].valid = 1;
		arpcache.entries[free_id].added = time(NULL);
		memcpy(arpcache.entries[free_id].mac, mac, ETH_ALEN);

		// send packets in arpcache seq
		list_for_each_entry_safe(pkt_entry, q_pkt, &(req_entry->cached_packets), list) {
			pthread_mutex_unlock(&arpcache.lock); 
			iface_send_packet_by_arp(req_entry->iface, ip4, pkt_entry->packet, pkt_entry->len);
			pthread_mutex_lock(&arpcache.lock); 
			
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry); 
		}
		free(req_entry->iface);
		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}
	pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
	struct arp_req *req_entry, *q_req;
	struct cached_pkt *pkt_entry, *q_pkt;

	while (1) {
		sleep(1);
		pthread_mutex_lock(&arpcache.lock);
		
		// delete entry that exist > 15s
		for (int i = 0; i < MAX_ARP_SIZE; i ++) {
			if (arpcache.entries[i].valid && time(NULL) - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT) {
				arpcache.entries[i].valid = 0;
				fprintf(stderr, "Sweep arpcache: remove entry out of time.\n");
			}
		}

		list_for_each_entry_safe(req_entry, q_req, &(arpcache.req_list), list) {
			if (req_entry->retries >= ARP_REQUEST_MAX_RETRIES) {
				list_for_each_entry_safe(pkt_entry, q_pkt, &(req_entry->cached_packets), list) {
					pthread_mutex_unlock(&arpcache.lock);
					icmp_send_packet(pkt_entry->packet, pkt_entry->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
					pthread_mutex_lock(&arpcache.lock);
					
					free(pkt_entry->packet);
					list_delete_entry(&(pkt_entry->list));
					free(pkt_entry);
				}
				free(req_entry->iface);
				list_delete_entry(&(req_entry->list));
				free(req_entry);
			} else {
				arp_send_request(req_entry->iface, req_entry->ip4);
				req_entry->retries += 1;
				req_entry->sent = time(NULL);
			}
		}
		pthread_mutex_unlock(&arpcache.lock);	
	}
	return NULL;
}
