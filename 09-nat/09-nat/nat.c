#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

//TODO
// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	fprintf(stdout, "Determine packet direction.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 src = ntohl(ip->saddr);
	u32 dst = ntohl(ip->daddr);
	rt_entry_t *s_entry = longest_prefix_match(src);
	rt_entry_t *d_entry = longest_prefix_match(dst);

	if (!strcmp(s_entry->iface->name, nat.internal_iface->name) && !strcmp(d_entry->iface->name, nat.external_iface->name))
		return DIR_OUT;
		
	if (!strcmp(s_entry->iface->name, nat.external_iface->name) && !strcmp(d_entry->iface->name, nat.external_iface->name))
		return DIR_IN;
	
	return DIR_INVALID;
}

u16 assign_external_port()
{
	u16 i = NAT_PORT_MIN;
	for (; i < NAT_PORT_MAX; i ++) {
		if (!nat.assigned_ports[i]) {
			nat.assigned_ports[i] = 1;
			return i;
		}
	}
	return -1;
}

// TODO
// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	fprintf(stdout, "Do translation.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = packet_to_tcp_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	u32 saddr = ntohl(ip->saddr);
	u8 key = (dir == DIR_OUT) ? hash8((char *)&daddr, sizeof(daddr)) : hash8((char *)&saddr, sizeof(saddr));
	struct list_head *head = &nat.nat_mapping_list[key];
	struct nat_mapping *mapping_entry, *entry;
	int found = 0;
	int fin = tcp->flags & TCP_FIN;
	int ack = tcp->flags & TCP_ACK;
	int rst = tcp->flags & TCP_RST;
	
	pthread_mutex_lock(&nat.lock);	
	if (dir == DIR_OUT) {
		list_for_each_entry_safe(mapping_entry, entry, head, list) {
			if (mapping_entry->external_ip == daddr) {
				found = 1;
				break;
			}
		}
		// build a new mapping
		if (!found) {
			entry = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
			entry->internal_ip = saddr;
			entry->external_ip = daddr;
			entry->internal_port = ntohs(tcp->sport);
			entry->external_port = assign_external_port();
			mapping_entry->update_time = time(NULL);
			bzero(&(entry->conn), sizeof(struct nat_connection));
			list_add_tail(&(entry->list), &(nat.nat_mapping_list[key]));
			mapping_entry = entry;
		}
		// update mapping
		mapping_entry->conn.internal_fin = fin | rst;
		mapping_entry->conn.internal_ack = ack | rst;
		mapping_entry->conn.external_fin = rst;
		mapping_entry->conn.external_ack = rst;

		ip->saddr = htonl(nat.external_iface->ip);
		tcp->sport = htons(mapping_entry->external_port);
	}
	else if (dir == DIR_IN) {
		found = 0;
		list_for_each_entry_safe(mapping_entry, entry, head, list) {
			if (mapping_entry->external_ip == saddr) {
				found = 1;
				break;
			}
		}
		if (!found) {
			printf("DIR_IN: non-recorded src.\n");

		}
		
		// update mapping
		mapping_entry->conn.internal_fin = fin;
		mapping_entry->conn.internal_ack = ack;
		mapping_entry->conn.external_fin = fin | rst;
		mapping_entry->conn.external_ack = ack | rst;
		// mapping_entry->update_time = time(NULL);

		ip->daddr = htonl(nat.rules);
		// tcp->dport = htons(nat.rules.dport);
		
	}
	ip->checksum = ip_checksum(ip);
	tcp->checksum = tcp_checksum(ip, tcp);
	ip_send_packet(packet, len);
	pthread_mutex_unlock(&nat.lock);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

//TODO
// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		pthread_mutex_lock(&nat.lock);
		for (int i = 0; i < HASH_8BITS; i ++) {
			struct list_head *head = &nat.nat_mapping_list[i];
			struct nat_mapping *mapping_entry, *entry;
			list_for_each_entry_safe(mapping_entry, entry, head, list) {
				mapping_entry->update_time += 1;
				if (time(NULL) - mapping_entry->update_time > TCP_ESTABLISHED_TIMEOUT || is_flow_finished(&mapping_entry->conn)) {
					nat.assigned_ports[mapping_entry->external_port] = 0;
					list_delete_entry(&mapping_entry->list);
					free(mapping_entry);
					fprintf(stdout, "Sweep aged connection.\n");
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}
	return NULL;
}

#define MAX_STR_SIZE 100
int get_next_line(FILE *input, char (*strs)[MAX_STR_SIZE], int *num_strings)
{
	const char *delim = " \t\n";
	char buffer[120];
	int ret = 0;
	if (fgets(buffer, sizeof(buffer), input)) {
		char *token = strtok(buffer, delim);
		*num_strings = 0;
		while (token) {
			strcpy(strs[(*num_strings)++], token);
			token = strtok(NULL, delim);
		}

		ret = 1;
	}

	return ret;
}

int read_ip_port(const char *str, u32 *ip, u16 *port)
{
	int i1, i2, i3, i4;
	int ret = sscanf(str, "%d.%d.%d.%d:%hu", &i1, &i2, &i3, &i4, port);
	if (ret != 5) {
		log(ERROR, "parse ip-port string error: %s.", str);
		exit(1);
	}

	*ip = (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;

	return 0;
}

int parse_config(const char *filename)
{
	FILE *input;
	char strings[10][MAX_STR_SIZE];
	int num_strings;

	input = fopen(filename, "r");
	if (input) {
		while (get_next_line(input, strings, &num_strings)) {
			if (num_strings == 0)
				continue;

			if (strcmp(strings[0], "internal-iface:") == 0)
				nat.internal_iface = if_name_to_iface("n1-eth0");
			else if (strcmp(strings[0], "external-iface:") == 0)
				nat.external_iface = if_name_to_iface("n1-eth1");
			else if (strcmp(strings[0], "dnat-rules:") == 0) {
				struct dnat_rule *rule = (struct dnat_rule*)malloc(sizeof(struct dnat_rule));
				read_ip_port(strings[1], &rule->external_ip, &rule->external_port);
				read_ip_port(strings[3], &rule->internal_ip, &rule->internal_port);
				
				list_add_tail(&rule->list, &nat.rules);
			}
			else {
				log(ERROR, "incorrect config file, exit.");
				exit(1);
			}
		}

		fclose(input);
	}
	else {
		log(ERROR, "could not find config file '%s', exit.", filename);
		exit(1);
	}
	
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

//TODO
void nat_exit()
{
	fprintf(stdout, "Release all resources.\n");
	pthread_mutex_lock(&nat.lock);
	for (int i = 0; i < HASH_8BITS; i ++) {
		struct list_head *head = &nat.nat_mapping_list[i];
		struct nat_mapping *mapping_entry, *entry;
		list_for_each_entry_safe(mapping_entry, entry, head, list) {
			list_delete_entry(&mapping_entry->list);
			free(mapping_entry);
		}
	}
	pthread_kill(nat.thread, SIGTERM);
	pthread_mutex_unlock(&nat.lock);

}
