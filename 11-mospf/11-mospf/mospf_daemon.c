#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"
#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

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

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	char dst_mac[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05};
	int pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
	iface_info_t *iface;
	
	while (1) {
		list_for_each_entry(iface, &instance->iface_list, list) {
			char *packet = (char *)malloc(sizeof(pkt_len));
			struct ether_header *eh = (struct ether_header *)packet;

			memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
			memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
			eh->ether_type = htons(ETH_P_IP);

			struct mospf_hello *hello = (struct mospf_hello *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE);
			mospf_init_hello(hello, iface->mask);
			
			struct mospf_hdr *mospf = (struct mospf_hdr *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE);
			mospf_init_hdr(mospf, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE, instance->router_id, instance->area_id);
			mospf->checksum = mospf_checksum(mospf);

			struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
			ip_init_hdr(ip, iface->ip, MOSPF_ALLSPFRouters, IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE, IPPROTO_MOSPF);

			iface_send_packet(iface, packet, pkt_len);
		}
		sleep(MOSPF_DEFAULT_HELLOINT);
	}
	return NULL;
}

void send_mospf_lsu()
{
	// ************* get nbr info *******************
	char dst_mac[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05};
	iface_info_t *iface;
	mospf_nbr_t *nbr;
	int nbr_num = 0, exist = 0;
	struct mospf_lsa lsa[nbr_num];
	int i = 0;

	list_for_each_entry(iface, &instance->iface_list, list) {
		if (iface->num_nbr)
			nbr_num += iface->num_nbr;
		else
			nbr_num ++;
	}
	list_for_each_entry(iface, &instance->iface_list, list) {
		list_for_each_entry(nbr, &iface->nbr_list, list) {
			lsa[i].subnet = htonl(nbr->nbr_ip & nbr->nbr_mask);
			lsa[i].mask = htonl(nbr->nbr_mask);
			lsa[i].rid = htonl(nbr->nbr_id);
			i ++;
			exist = 1;
		}
		if (!exist) {
			fprintf(stdout, "Add empty iface\n");
			lsa[i].subnet = htonl(nbr->nbr_ip & nbr->nbr_mask);
			lsa[i].mask = htonl(nbr->nbr_mask);
			lsa[i].rid = htonl(0);
			i ++;
		}
	}
	
	// ************* send lsu pkt *******************
	int pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_num * sizeof(struct mospf_lsa);
	list_for_each_entry(iface, &instance->iface_list, list) {
		char *packet = (char *)malloc(pkt_len);
		struct iphdr *ip = (struct iphdr *)(packet+ETHER_HDR_SIZE);
		ip_init_hdr(ip, iface->ip, nbr->nbr_ip, IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_LSU_SIZE+nbr_num*sizeof(struct mospf_lsa), IPPROTO_MOSPF);

		struct mospf_hdr *mospf = (struct mospf_hdr *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE);
		mospf_init_hdr(mospf, MOSPF_TYPE_LSU, MOSPF_HDR_SIZE+MOSPF_LSU_SIZE + nbr_num*sizeof(struct mospf_lsa), instance->router_id, instance->area_id);

		struct mospf_lsu *lsu = (struct mospf_lsu *)(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE);
		mospf_init_lsu(lsu, nbr_num);

		memcpy(packet+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_LSU_SIZE, lsa, nbr_num*sizeof(struct mospf_lsa));
		mospf->checksum = mospf_checksum(mospf);
		instance->sequence_num ++;
		
		ip_send_packet(packet, pkt_len);
	}
}

void *checking_nbr_thread(void *param)
{
	iface_info_t *iface;
	mospf_nbr_t *nbr, *q;
	while (1) {
		pthread_mutex_lock(&mospf_lock);
		fprintf(stdout, "TODO: neighbor list timeout operation.\n");
		list_for_each_entry(iface, &instance->iface_list, list) {
			list_for_each_entry_safe(nbr, q, &iface->nbr_list, list) {
				nbr->alive ++;
				if (nbr->alive > MOSPF_DEFAULT_HELLOINT * 3) {
					list_delete_entry(&nbr->list);
					iface->num_nbr --;
					send_mospf_lsu();
					free(nbr);
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
	mospf_db_entry_t *db, *q;
	while (1) {
		pthread_mutex_lock(&mospf_lock);
		fprintf(stdout, "TODO: database timeout operation.\n");
		list_for_each_entry_safe(db, q, &mospf_db, list) {
				db->alive ++;
				if (db->alive > MOSPF_DATABASE_TIMEOUT) {
					list_delete_entry(&db->list);
					free(db);
				}
		}
		pthread_mutex_unlock(&mospf_lock);
		sleep(1);
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct mospf_hello *hello = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	mospf_nbr_t *nbr;
	int found = 0;
	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(nbr, &iface->nbr_list, list) {
		if (nbr->nbr_id == ntohl(mospf->rid)) {
			found = 1;
			nbr->alive = 0;
			break;
		}
	}
	if (!found) {
		nbr = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
		nbr->nbr_id = ntohl(mospf->rid);
		nbr->nbr_ip = ntohl(ip->saddr);
		nbr->nbr_mask = ntohl(hello->mask);
		nbr->alive = 0;
		list_add_tail(&nbr->list, &iface->nbr_list);
		iface->num_nbr ++;
	}
	pthread_mutex_unlock(&mospf_lock);
}

void *sending_mospf_lsu_thread(void *param)
{
	while (1) {
		fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
		send_mospf_lsu();
		sleep(instance->lsuint);
	}
	
	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct mospf_hdr *mospf = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_lsa *lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
	int found = 0, new = 0;
	mospf_db_entry_t *db;
	mospf_nbr_t *nbr;
	u32 rid = ntohl(mospf->rid);
	u32 seq = ntohs(lsu->seq);
	u32 nadv = ntohl(lsu->nadv);

	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(db, &mospf_db, list) {
		if (db->rid == rid) {
			found = 1;
			db->alive = 0;
			if (db->seq < seq) {
				db->seq = seq;
				db->nadv = nadv;
				int i = 0;
				while (i < db->nadv) {
					db->array[i].subnet = ntohl(lsa[i].subnet);
					db->array[i].mask = ntohl(lsa[i].mask);
					db->array[i].rid = ntohl(lsa[i].rid);
					fprintf(stdout, "update db entry\n");
				}
				new = 1;
			}
			break;
		}
	}
	if (!found) {
		db = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		db->rid = rid;
		db->seq = seq;
		db->nadv = nadv;
		db->alive = 0;
		db->array = (struct mospf_lsa *)malloc(sizeof(struct mospf_lsa)*nadv);
		for (int i = 0; i < nadv; i ++) {
			db->array[i].subnet = ntohl(lsa[i].subnet);
			db->array[i].mask = ntohl(lsa[i].mask);
			db->array[i].rid = ntohl(lsa[i].rid);
			fprintf(stdout, "add new db entry\n");
		}
		list_add_tail(&db->list, &mospf_db);
		new = 1;
	}
	if (lsu->ttl >= 0 && new) {
		mospf->checksum = mospf_checksum(mospf);
		list_for_each_entry(iface, &instance->iface_list, list) {
			list_for_each_entry(nbr, &iface->nbr_list, list) {
				if (ntohl(ip->saddr)==nbr->nbr_ip || rid==nbr->nbr_id)
					continue;
				char *pkt = (char *)malloc(len);
				memcpy(pkt, packet, len); // ### 省略newip
				ip_send_packet(pkt, len);
			}
		}
	}
	pthread_mutex_unlock(&mospf_lock);
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

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
