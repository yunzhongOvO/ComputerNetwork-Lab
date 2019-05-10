#include "types.h"
#include <stdint.h>

#define CHILD_NUM 2
#define BIT_STEP 4  // optional, BIT_STEP = 1, 2, 3, 4
#define MAX_KEY (1<<BIT_STEP)

u32 node_num;
int cnt;

// *************** naive trie tree ***********************

typedef struct trie
{
    struct trie *child[CHILD_NUM];
    u8 match;
} Trie_node_t;

Trie_node_t* new_trie_node(u32 match);
void insert_trie_node(Trie_node_t *trie, u32 ip, u32 mask_len);
u32 lookup_trie_node(Trie_node_t *trie, u32 ip);
void destroy_tree(Trie_node_t *trie);

//******************* multi-bit trie **********************

typedef struct multi_trie
{
    struct multi_trie *child[1 << BIT_STEP];
    u16 match;
	u8 mask[1 << BIT_STEP];
} Multi_trie_t;

Multi_trie_t * new_multi_trie_node(u32 match);
void insert_multi_trie_node(Multi_trie_t *trie, u32 ip, u32 mask_len);
u32 lookup_multi_trie_node(Multi_trie_t *trie, u32 ip);


// ****************** poptrie ***************************

#define popcnt(v) __builtin_popcountll((u32)v)
typedef struct poptrie_node
{
    struct poptrie_node *child[1 << BIT_STEP];
    u16 match;
	u8 mask[1 << BIT_STEP];
} Poptrie_node_t;

typedef struct poptrie
{
	u16 vec;
	u16 leafvec;
	u32 base0;
	u32 base1;
} Poptrie_t;

typedef u8 Poptrie_leaf_t;
Poptrie_node_t *poptrie;
Poptrie_t *Poptrie;
Poptrie_leaf_t *L;
Poptrie_t *N;

Poptrie_node_t * new_poptrie_node();
void insert_poptrie_node(u32 ip, u32 mask_len);
void count_node_leaf(Poptrie_node_t *trie);
void compress_poptrie(Poptrie_node_t *trie, Poptrie_t *pop_trie);
void build_poptrie(Poptrie_node_t *trie);
u32 lookup_poptrie_node(u32 ip);


// *********************** other ****************************

#define IP_FMT	"%u.%u.%u.%u"
#define LE_IP_FMT_STR(ip) ((u8 *)&(ip))[3], \
						  ((u8 *)&(ip))[2], \
 						  ((u8 *)&(ip))[1], \
					      ((u8 *)&(ip))[0]

#define BE_IP_FMT_STR(ip) ((u8 *)&(ip))[0], \
						  ((u8 *)&(ip))[1], \
 						  ((u8 *)&(ip))[2], \
					      ((u8 *)&(ip))[3]

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define HOST_IP_FMT_STR(ip)	LE_IP_FMT_STR(ip)
#elif __BYTE_ORDER == __BIG_ENDIAN
#	define HOST_IP_FMT_STR(ip)	BE_IP_FMT_STR(ip)
#endif
