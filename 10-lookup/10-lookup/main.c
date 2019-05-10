#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "trie.h"

u32 ip_buf[700000];

u32 parse_ip_port(char *str)
{
	int i1, i2, i3, i4;
    sscanf(str, "%d.%d.%d.%d", &i1, &i2, &i3, &i4);
	return (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;
}

int main()
{
    cnt = 0;
    int port;
    u32 ip, mask;
    char ip_str[512];
    int i;
    double total;
    struct timeval t1, t2;
    u32 found;
    FILE *file, *res;
    // ********************** Naive trie ********************  
    printf("************* Naive trie ****************\n");
    // input file
    file = fopen("forwarding-table.txt", "r");
    // FILE *file = fopen("sub-table.txt", "r");
    Trie_node_t *trie = new_trie_node(0);
    // build trie tree
    while (!feof(file))
    {
        if (fscanf(file, "%s %u %d", ip_str, &mask, &port) < 0)
            break;
        ip = parse_ip_port(ip_str);
        insert_trie_node(trie, ip, mask);
        ip_buf[cnt++] = ip;
    }    
    fclose(file);

    // compute space overhead
    printf(" [Space Overhead] %lu MB\n", sizeof(Trie_node_t)*node_num/1048576);

    // look up
    res = fopen("trie-result.txt", "w+");
    
    gettimeofday(&t1, NULL);
    for (i = 0; i < cnt; i ++) {
        found = lookup_trie_node(trie, ip_buf[i]);
        // fprintf(res, IP_FMT"\t%u\n", HOST_IP_FMT_STR(ip_buf[i]), found);
    }
    gettimeofday(&t2, NULL);

    fclose(res);
    total = (t2.tv_sec-t1.tv_sec)*1000000+t2.tv_usec-t1.tv_usec;
    printf(" [Average Time Overhead] %lf us\n\n", total/cnt);


    // ********************** Muiti-bit trie ********************  

    printf("************* Muiti-bit trie ****************\n [BIT STEP] %d\n", BIT_STEP);
    
    // input file
    file = fopen("forwarding-table.txt", "r");
    // file = fopen("sub-table.txt", "r");
    Multi_trie_t *multi_trie = new_multi_trie_node(0);
    cnt = 0;
    node_num = 0;

    /// build trie tree
    while (!feof(file))
    {
        if (fscanf(file, "%s %u %d", ip_str, &mask, &port) < 0)
            break;
        ip = parse_ip_port(ip_str);
        insert_multi_trie_node(multi_trie, ip, mask);
        ip_buf[cnt++] = ip;
    } 
    fclose(file);

    // compute space overhead
    printf(" [Space Overhead] %lu MB\n", sizeof(Trie_node_t)*node_num/1048576);

    // look up
    res = fopen("multi-result.txt", "w+");
    gettimeofday(&t1, NULL);
    for (i = 0; i < cnt; i ++) {
        found = lookup_multi_trie_node(multi_trie, ip_buf[i]);
        // fprintf(res, IP_FMT"\t%u\n", HOST_IP_FMT_STR(ip_buf[i]), found);
    }
    gettimeofday(&t2, NULL);

    fclose(res);
    total = (t2.tv_sec-t1.tv_sec)*1000000+t2.tv_usec-t1.tv_usec;
    printf(" [Average Search Time] %lf us\n\n", total/cnt);
    
    
    // ********************** Poptrie ********************  

    // printf("************* Poptrie ****************\n [BIT STEP] %d\n", BIT_STEP);
    
    // file = fopen("forwarding-table.txt", "r");

    // poptrie = new_poptrie_node();
    // while (!feof(file))
    // {
    //     if (fscanf(file, "%s %u %d", ip_str, &mask, &port) < 0)
    //         break;
    //     ip = parse_ip_port(ip_str);
    //     insert_poptrie_node(ip, mask);
    //     ip_buf[cnt++] = ip;
    // }
    // fclose(file);

    // build_poptrie(poptrie);

    // res = fopen("poptrie-result.txt", "w+");

    // gettimeofday(&t1, NULL);
    // for(int i = 0; i < cnt; i++)
    // {
    //     u32 p = lookup_poptrie_node(ip_buf[i]);
    //     fprintf(res, IP_FMT"\t%u\n", HOST_IP_FMT_STR(ip), p);
    // }
    // gettimeofday(&t2, NULL);
    // total = (t2.tv_sec-t1.tv_sec)*1000000+t2.tv_usec-t1.tv_usec;
    // printf(" [Average Search Time] %lf us\n\n", total/cnt);
    // fclose(res);

    // compare results
    printf("*********** Compare Results ***********\n");
    FILE *diff = popen("diff trie-result.txt multi-result.txt", "r");
    char ch = fgetc(diff);
    if (ch == EOF)
        printf(" [Poptrie] Correct\n");
    else
        printf("%c\n", ch);
    pclose(diff);

    return 0;
}