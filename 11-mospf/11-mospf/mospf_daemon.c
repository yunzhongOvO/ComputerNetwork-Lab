#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"
#include "list.h"
#include "log.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;
static char hello_mac[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05};

pthread_mutex_t mospf_lock;
void build_router();

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param); // ##

// void build_router();

void print_db()
{
	printf("------------- database ------------\n");
	mospf_db_entry_t *db;
	list_for_each_entry(db, &mospf_db, list) {
		for (int i = 0; i < db->nadv; i ++) {
			fprintf(stdout, IP_FMT"  "IP_FMT"  "IP_FMT"  "IP_FMT"\n",
               HOST_IP_FMT_STR(db->rid),
               HOST_IP_FMT_STR(db->array[i].subnet),
               HOST_IP_FMT_STR(db->array[i].mask),
               HOST_IP_FMT_STR(db->array[i].rid));
		}
	}
	printf("\n");
}

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void mospf_send_hello(u32 rid, u32 mask, u8 *src_mac, u32 src_ip, iface_info_t* iface) 
{
	int packet_len = ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE;
	char *packet = (char *)malloc(packet_len);
	struct ether_header *ether = (struct ether_header *)packet;
	memcpy(ether->ether_dhost, hello_mac, ETH_ALEN);
	memcpy(ether->ether_shost, src_mac, ETH_ALEN);
	ether->ether_type = htons(ETH_P_IP);

	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	ip_init_hdr(ip, src_ip, MOSPF_ALLSPFRouters, IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, IPPROTO_MOSPF);

	struct mospf_hdr *mospf = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	mospf_init_hdr(mospf, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, rid, instance->area_id);

	struct mospf_hello *hello = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
	mospf_init_hello(hello, mask);
	mospf->checksum = mospf_checksum(mospf);

	iface_send_packet(iface, packet, packet_len);
}

void *sending_mospf_hello_thread(void *param)
{
	while (1) {
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list) {
			mospf_send_hello(instance->router_id, iface->mask, iface->mac, iface->ip, iface);
		}
		sleep(MOSPF_DEFAULT_HELLOINT);
	}
	return NULL;
}

void mospf_send_lsu()
{
	// ************* get nbr info *******************
		int num_nbr = 0;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list) {
			if (iface->num_nbr > 0) num_nbr += iface->num_nbr;
			else num_nbr++; 
		}
		struct mospf_lsa lsa[num_nbr];
		int nbr_info_idx = 0;
		list_for_each_entry(iface, &instance->iface_list, list) {
			mospf_nbr_t *nbr = NULL;
			int none = 1;
			list_for_each_entry(nbr, &iface->nbr_list, list) {
				lsa[nbr_info_idx].subnet = htonl(nbr->nbr_ip & nbr->nbr_mask);
				lsa[nbr_info_idx].mask = htonl(nbr->nbr_mask);
				lsa[nbr_info_idx].rid = htonl(nbr->nbr_id);
				nbr_info_idx++;
				none = 0;
			}
			if (none) { 
				fprintf(stdout, "Empty Nbr Iface"IP_FMT"  "IP_FMT"\n",
		               HOST_IP_FMT_STR(iface->ip),
		               HOST_IP_FMT_STR(iface->mask));
				lsa[nbr_info_idx].subnet = htonl(iface->ip & iface->mask);
				lsa[nbr_info_idx].mask = htonl(iface->mask);
				lsa[nbr_info_idx].rid = htonl(0); 
				nbr_info_idx++;
			}
		}

	// ************* send lsu pkt *******************
	iface = NULL;
	instance->sequence_num++;
	int packet_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + num_nbr * sizeof(struct mospf_lsa);
	list_for_each_entry(iface, &instance->iface_list, list) {
		mospf_nbr_t *nbr = NULL;
		list_for_each_entry(nbr, &iface->nbr_list, list) {
			char *packet = (char *)malloc(packet_len);
			struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
			ip_init_hdr(ip, iface->ip, nbr->nbr_ip, packet_len - ETHER_HDR_SIZE, IPPROTO_MOSPF);

			struct mospf_hdr *mospf = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
			mospf_init_hdr(mospf, MOSPF_TYPE_LSU, packet_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE, instance->router_id, instance->area_id);

			struct mospf_lsu *lsu = (struct mospf_lsu *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
			mospf_init_lsu(lsu, num_nbr);

			memcpy(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE, lsa, num_nbr * sizeof(struct mospf_lsa));

			mospf->checksum = mospf_checksum(mospf);
			ip_send_packet(packet, packet_len);
		}
	}
}

void *checking_nbr_thread(void *param)
{
	while (1) {
		iface_info_t *iface = NULL;
			list_for_each_entry(iface, &instance->iface_list, list) {
				mospf_nbr_t *nbr = NULL, *nbr_q = NULL;
				list_for_each_entry_safe(nbr, nbr_q, &iface->nbr_list, list) {
					nbr->alive++;
					if (nbr->alive > MOSPF_DEFAULT_HELLOINT * 3) {
						list_delete_entry(&(nbr->list));
                        iface->num_nbr--;
                        mospf_send_lsu();
					}
				}
			}
		pthread_mutex_unlock(&mospf_lock);
		sleep(1);
	}
	return NULL;
}

void *checking_database_thread(void *param)
{
	while (1) {
		mospf_db_entry_t *db = NULL, *entry_q= NULL;
			list_for_each_entry_safe(db, entry_q, &mospf_db, list) {
				db->alive++;
				if (db->alive > MOSPF_DATABASE_TIMEOUT) {
					list_delete_entry(&(db->list));
				}
			}
		print_db();
		build_router();
		print_rtable();
		pthread_mutex_unlock(&mospf_lock);
		sleep(1);
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ip = (struct iphdr *)(packet+ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE);
	struct mospf_hello *hello = (struct mospf_hello *)(packet + ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE);
	int found = 0;
	mospf_nbr_t *nbr;
	pthread_mutex_lock(&mospf_lock);
		list_for_each_entry(nbr, &iface->nbr_list, list) {
			if (ntohl(mospf->rid) == nbr->nbr_id) {
				found = 1;
				nbr->alive = 0;
				break;
			}
		}
		if (!found){
			mospf_nbr_t *new = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
			new->nbr_id = ntohl(mospf->rid);
			new->nbr_ip = ntohl(ip->saddr);
			new->nbr_mask = ntohl(hello->mask);
			new->alive = 0;
			iface->num_nbr ++;
			list_add_tail(&(new->list), &(iface->nbr_list));
			pthread_mutex_unlock(&mospf_lock);

			mospf_send_lsu();
		}
	pthread_mutex_unlock(&mospf_lock);
}

void *sending_mospf_lsu_thread(void *param)
{
	while (1) {
		mospf_send_lsu();
		fprintf(stdout, "send mOSPF LSU message periodically.\n");
		sleep(instance->lsuint);
	}
	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet+ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE);
	struct mospf_lsa *lsa = (struct mospf_lsa *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_LSU_SIZE);
	
	pthread_mutex_lock(&mospf_lock);
		int found = 0, is_update = 0;
		mospf_db_entry_t *db;
		list_for_each_entry(db, &mospf_db, list) {
			if (db->rid == ntohl(mospf->rid)) {
				found = 1;
				db->alive = 0;
				if (db->seq < ntohs(lsu->seq)) {
					db->seq = ntohs(lsu->seq);
					db->nadv = ntohl(lsu->nadv);
					for (int i = 0; i < db->nadv; i++) {
						db->array[i].subnet = ntohl(lsa[i].subnet);
						db->array[i].mask = ntohl(lsa[i].mask);
						db->array[i].rid = ntohl(lsa[i].rid);
						fprintf(stdout, "[Update db] "IP_FMT"  "IP_FMT"  "IP_FMT"  "IP_FMT"\n",
				               HOST_IP_FMT_STR(db->rid),
				               HOST_IP_FMT_STR(db->array[i].subnet),
				               HOST_IP_FMT_STR(db->array[i].mask),
				               HOST_IP_FMT_STR(db->array[i].rid));
					}
					is_update = 1;
				}
				break;
			}
		}
		if (!found) {
			mospf_db_entry_t *entry = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
			entry->rid = ntohl(mospf->rid);
			entry->seq = ntohs(lsu->seq);
			entry->nadv = ntohl(lsu->nadv);
			entry->alive = 0;
			entry->array = (struct mospf_lsa*)malloc(entry->nadv * sizeof(struct mospf_lsa));
			for (int i = 0; i < entry->nadv; i ++) {
				entry->array[i].subnet = ntohl(lsa[i].subnet);
				entry->array[i].mask = ntohl(lsa[i].mask);
				entry->array[i].rid = ntohl(lsa[i].rid);
				fprintf(stdout, "Add new db entry "IP_FMT"  "IP_FMT"  "IP_FMT"  "IP_FMT"\n",
		               HOST_IP_FMT_STR(entry->rid),
		               HOST_IP_FMT_STR(entry->array[i].subnet),
		               HOST_IP_FMT_STR(entry->array[i].mask),
		               HOST_IP_FMT_STR(entry->array[i].rid));
			}
			is_update = 1;
			list_add_tail(&entry->list, &mospf_db);
		}
	pthread_mutex_unlock(&mospf_lock);

	if (is_update > 0) {
		if (--lsu->ttl > 0) {
			mospf->checksum = mospf_checksum(mospf);
			iface_info_t *iface;
			list_for_each_entry(iface, &instance->iface_list, list) {
				mospf_nbr_t *nbr;
				list_for_each_entry(nbr, &iface->nbr_list, list) {
					if (nbr->nbr_ip == ntohl(ip->saddr)) continue;
					if (nbr->nbr_id == ntohl(mospf->rid)) continue;

					char *new_packet = (char *)malloc(len);
					memcpy(new_packet, packet, len);

					struct iphdr *new_ip = (struct iphdr *)(new_packet + ETHER_HDR_SIZE);
					new_ip->saddr = htonl(iface->ip);
					new_ip->daddr = htonl(nbr->nbr_ip);
					new_ip->checksum = ip_checksum(new_ip);

					ip_send_packet(new_packet, len);
				}
			}
		}
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + (ip->ihl * 4));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}



// ************** generate router *******************

#define MAX_NUM 255
#define INF 255
int graph_matrix[MAX_NUM][MAX_NUM]; // adjacency matrix of graph
int visited[MAX_NUM];	// if node[i] is visited
int prev[MAX_NUM];		// last hop in route from node[0] to node[i]
int hops[MAX_NUM];		// shortest hops from node[0] to node[i]
u32 id_order[MAX_NUM];	// id of each node
int noden;				// number of nodes

int get_node_order(u32 rid) {
	for (int i = 0; i < noden; i++) {
		if (id_order[i] == rid)
			return i;
	}
	return -1;
}

void build_graph() 
{
	mospf_db_entry_t *db;
	list_for_each_entry(db, &mospf_db, list) {
		for (int i = 0; i < db->nadv; i++) {
			if (db->array[i].rid == 0) continue;
			int id1 = get_node_order(db->rid);
			int id2 = get_node_order(db->array[i].rid);
			graph_matrix[id1][id2] = 1;
			graph_matrix[id2][id1] = 1;
		}
	}
}

void dijkstra()
{
	for (int i = 0; i < noden; i ++) {
		hops[i] = INF;
		prev[i] = -1;
	}
	hops[0] = 0;
	for (int i = 0; i < noden; i ++) {
		int min_hops = INF;
		int index = -1;
		for (int j = 0; j < noden; j++) {
			if (visited[j] == 0 && hops[j] < min_hops) {
				min_hops = hops[j];
				index = j;
			}
		}
		if (index < 0) break;
		visited[index] = 1;
		for (int v = 0; v < noden; v++) {
			if (!visited[v] && graph_matrix[index][v] && \
			hops[index] + graph_matrix[index][v] < hops[v]) {
				hops[v] = hops[index] + graph_matrix[index][v];
				prev[v] = index;
			}
		}
	}
	printf("[shortest path]\n");
	for (int i = 0; i < noden; i++) {
		printf("dijkstra: %d->%o hops: %d prev: %d\n", i, id_order[i], hops[i], prev[i]);
	}
}

int find_src_node(int dst) 
{
	while (prev[dst] != 0) {
		dst = prev[dst];
	}
	return dst;
}

rt_entry_t* check_router(u32 dst) 
{
	rt_entry_t *db = NULL;
	list_for_each_entry(db, &rtable, list) {
		if ((dst & db->mask) == (db->dest & db->mask)) {
			if (db->gw != 0) 
				return db;
		}
	}
	return NULL;
}

// void clear_mospf_rib() {
	
// }

void update_router() 
{
	int min_hops, min_hops_index;
	rt_entry_t *entry, *q;
	visited[0] = 1;
	bzero(visited, sizeof(visited));
	list_for_each_entry_safe(entry, q, &rtable, list) {
		if (entry->gw != 0) {
			remove_rt_entry(entry);
		}
	}
	while (1) {
		min_hops = INF;
		min_hops_index = -1;
		for (int i = 0; i < noden;  ++) {
			if (!visited[i] && hops[i] < min_hops) {
				min_hops = hops[i];
				min_hops_index = i;
			}
		}
		if (min_hops_index < 0) break;
		visited[min_hops_index] = 1;
		
		mospf_db_entry_t *db;
		list_for_each_entry(db, &mospf_db, list) {
			if (db->rid == id_order[min_hops_index]) {
				int next_hop_id = find_src_node(min_hops_index);
				iface_info_t *iface, *src;
				u32 dst_gw = 0;
				int found = 0;
				list_for_each_entry(iface, &instance->iface_list, list) {
					mospf_nbr_t *nbr;
					list_for_each_entry(nbr, &iface->nbr_list, list) {
						if (nbr->nbr_id == id_order[next_hop_id]) {
							found = 1;
							dst_gw = nbr->nbr_ip;
							src = iface;
							break;
						}
					}
					if (found) break;
				}
				if (found == 0)  break;
				for (int i = 0; i < db->nadv; i ++) {
					rt_entry_t *new = check_router(db->array[i].subnet);
					if (new) 	continue;
					new = new_rt_entry(db->array[i].subnet, db->array[i].mask, dst_gw, src);
					printf("[update router] add new entry: %o -> %u %s\n", db->array[i].subnet, dst_gw, src->name);
					add_rt_entry(new);
				}
				break;
			}
		}
	}
}

void build_router() 
{
	// init graph adjacency matrix
	bzero(graph_matrix, sizeof(graph_matrix));
	bzero(visited, sizeof(visited));
	id_order[0] = instance->router_id;
	noden = 1;
	mospf_db_entry_t *db;

	list_for_each_entry(db, &mospf_db, list) {
		id_order[noden++] = db->rid;
	}
	build_graph();
	dijkstra();
	update_router();
}