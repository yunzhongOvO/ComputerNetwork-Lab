#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"

u32 noden = 0, leafn = 0;
u32 allocated_node = 1, allocated_leaf = 0;

Poptrie_node_t * new_poptrie_node(u32 match)
{
    Poptrie_node_t *poptrie = (Poptrie_node_t *)malloc(sizeof(Poptrie_node_t));
    poptrie->match = match;
    memset(poptrie->mask, 0, sizeof(poptrie->mask));
    for (int i = 0; i < (1 << BIT_STEP); i ++) {
        poptrie->child[i] = NULL;
    }
    return poptrie;
}

void insert_poptrie_node(u32 ip, u32 mask_len)
{
    Poptrie_node_t *p = poptrie;
    u32 bits = 0, tmp = 1;
    int i = BIT_STEP;
    for ( ; i < mask_len; i += BIT_STEP) {
        bits = (ip >> (32 - i)) & (0xF >> (4 - BIT_STEP));
        if (!p->child[bits])
            p->child[bits] = new_poptrie_node(0);
        p = p->child[bits];
    }
    bits = (ip >> (32 - i)) & (0xF >> (4 - BIT_STEP));
    for (int j = 0; j < 4 - mask_len % 4 && mask_len % 4; j ++) {
        tmp *= 2;
    }
    for (u32 j = 0; j < tmp; j ++) {
        p->match |= (1 << (bits + j));
        if (p->mask[bits+j] < mask_len)
            p->mask[bits+j] = mask_len;
    }
}

void count_node_leaf(Poptrie_node_t *poptrie)
{
    for (int i = 0; i < (1 << BIT_STEP); i ++) {
        if (poptrie->child[i])
            count_node_leaf(poptrie->child[i]);
    }
    noden += 1;
    leafn += popcnt(poptrie->match);
}

void compress_poptrie(Poptrie_node_t *poptrie, Poptrie_t *pop_trie)
{
    u32 bits;
    pop_trie->base0 = allocated_leaf;
    pop_trie->base1 = allocated_node;
    for (int i = 0; i < (1 << BIT_STEP); i ++) {
        if (poptrie->match & (1 << i)) {
            pop_trie->leafvec |= 1 << i;
            allocated_leaf ++;
            L[allocated_leaf-1] = poptrie->mask[i];
        }
        if (poptrie->child[i]){
            pop_trie->vec |= 1 << i;
            allocated_node ++;
        }
    }
    for (int i = 0; i < (1 << BIT_STEP); i ++) {
        if (poptrie->child[i]){
            bits = pop_trie->base1 + popcnt(pop_trie->vec & (((u32)2 << i) - 1)) - 1;
            compress_poptrie(poptrie->child[i], &N[bits]);
        }
    }
}

void build_poptrie(Poptrie_node_t *poptrie)
{
    count_node_leaf(poptrie);
    
    N = (Poptrie_t *)malloc(sizeof(Poptrie_t) * noden);
    L = (Poptrie_leaf_t *)malloc(sizeof(Poptrie_leaf_t) * leafn);

    printf(" [Size of Poptrie] %lu MB\n", (sizeof(Poptrie_t)*noden+sizeof(Poptrie_t)*leafn)/1048576);
    bzero(L, sizeof(L));
    bzero(N, sizeof(N));
    Poptrie = N;
    compress_poptrie(poptrie, Poptrie);
}

u32 lookup_poptrie_node(u32 ip)
{
    // Poptrie_node_t *p = poptrie;
    u16 vec = N[0].vec;
    u16 v = (ip >> (32 - BIT_STEP)) & 0xF;
    u32 offset = 0, bits = 0, num, res = 0;
    while (vec & (1ULL << v)) {
        if (N[bits].leafvec & (1ULL << v)) {
            res = L[N[bits].base0 + popcnt(N[bits].leafvec & ((2ULL << v) - 1)) - 1];
        }
        bits = N[bits].base1 + popcnt(vec & ((2 << v) - 1)) - 1;
        vec = N[bits].vec;
        offset += BIT_STEP;
        v = (ip >> (32 - BIT_STEP - offset)) & 0xF;

    }
    if (N[bits].leafvec & (1ULL << v)) {
        res = L[N[bits].base0 + popcnt(N[bits].leafvec & ((2ULL << v) - 1)) - 1];
    }
    return res;
}
