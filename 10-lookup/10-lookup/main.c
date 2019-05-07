#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "trie.h"

int read_ip_port(FILE *file, u32 *ip, u32 *mask)
{
	int i1, i2, i3, i4;
    int port;
	int ret = fscanf(file, "%d.%d.%d.%d %u %d", &i1, &i2, &i3, &i4, mask, &port);
    if (ret != 6) {
		// printf("[parse ip-port] error: ret = %d.\n", ret);
		return 0;
	}
	*ip = (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;
	return 1;
}

u32 ip_buf[700000];

int main()
{
    // input file
    // FILE *file = fopen("forwarding-table.txt", "r");
    FILE *file = fopen("sub-table.txt", "r");
    Trie_node_t *trie = new_trie_node(0);
    int cnt = 0;
    u32 ip, mask;

    printf("---- Naive trie tree ----\n");
    // insert trie node
    while (read_ip_port(file, &ip, &mask)) {
        insert_trie_node(trie, ip, mask);
        ip_buf[cnt++] = ip;
    }
    fclose(file);

    // compute space overhead
    printf(" [Space Overhead] %lu Bytes\n", sizeof(Trie_node_t) * node_num);

    // look up
    FILE *res = fopen("result.txt", "w+");
    struct timeval t1, t2;
    int found;
    gettimeofday(&t1, NULL);
    for (int i = 0; i < cnt; i ++) {
        found = lookup_trie_node(trie, ip_buf[i]);
        fprintf(res, IP_FMT" %u\n", HOST_IP_FMT_STR(ip), found);
    }
    gettimeofday(&t2, NULL);

    fclose(res);
    printf(" [Average Search Time] %2lfns\n", 1000.0*((t2.tv_sec-t1.tv_sec)*1000000+t2.tv_usec-t1.tv_usec)/cnt);





    return 0;
}