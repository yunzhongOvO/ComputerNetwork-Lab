#include <stdio.h>
#include <stdlib.h>
#include "trie.h"

Trie_node_t* new_trie_node(u32 match)
{
    Trie_node_t *trie = (Trie_node_t *)malloc(sizeof(Trie_node_t));
    node_num ++;
    trie->match = match;
    for (int i = 0; i < (1 << 1); i ++) {
        trie->child[i] = NULL;
    }
    return trie;
}

void insert_trie_node(Trie_node_t *trie, u32 ip, u32 mask_len)
{
    Trie_node_t *p = trie;
    u32 bit = 0;
    // printf("insert begin \n");
    for (int i = 0; i < mask_len; i ++) {
        bit = (ip >> (31 - i)) & 0x1;
        if (!p->child[bit])
            p->child[bit] = new_trie_node(0);
        p = p->child[bit];
    }

    if (p->match) {
        // printf("[insert trie node] node already exist.\n");
        return;
    }
    p->match = 1;
    // return;
}

u32 lookup_trie_node(Trie_node_t *trie, u32 ip)
{
    Trie_node_t *p = trie;
    u32 found = 0, bit = 0;
    for (int i = 0; p != NULL; i ++) {
        found = p->match;
        bit = (ip >> (31 - i)) & 0x1;
        p = p->child[bit];
    }
    return found;
}

void destroy_tree(Trie_node_t *trie)
{
    if (!trie)  return;
    for (int i = 0; i < CHILD_NUM; i ++) {
		if (trie->child[i]) {
			destroy_tree(trie->child[i]);
		}
	}
	free(trie);
}