#include "trie.h"
#include <stdio.h>
#include <stdlib.h>

Multi_trie_t * new_multi_trie_node(u32 match)
{
    Multi_trie_t *trie = (Multi_trie_t *)malloc(sizeof(Multi_trie_t));
    node_num ++;
    trie->match = match;
    for (int i = 0; i < (1 << BIT_STEP); i ++) {
        trie->child[i] = NULL;
    }
    return trie;
}

void insert_multi_trie_node(Multi_trie_t *trie, u32 ip, u32 mask_len)
{
    Multi_trie_t *p = trie;
    u32 bits = 0;
    for (int i = 0; i < mask_len; i += BIT_STEP) {
        bits = (ip >> (32 - BIT_STEP - i)) & (0xF >> (4 - BIT_STEP));
        if (!p->child[bits])
            p->child[bits] = new_multi_trie_node(0);
        p = p->child[bits];
    }
    if (p->match) {
        // printf("[insert trie node] node already exist.\n");
        return;
    }
    p->match = 1;
}

u32 lookup_multi_trie_node(Multi_trie_t *trie, u32 ip)
{
    Multi_trie_t *p = trie;
    u32 bits = 0;
    u32 found = 0;
    for (int i = 0; p != NULL; i += BIT_STEP) {
        found = p->match;
        bits = (ip >> (32 - BIT_STEP - i)) & (0xF >> (4 - BIT_STEP));
        p = p->child[bits];
    }
    return found;
}
