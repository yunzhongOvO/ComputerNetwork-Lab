#include "types.h"

#define CHILD_NUM 2

typedef struct trie
{
    struct trie *child[CHILD_NUM];
    u8 match;
} Trie_node_t;

u32 node_num;

Trie_node_t* new_trie_node(u32 match);
void insert_trie_node(Trie_node_t *trie, u32 ip, u32 mask_len);
int lookup_trie_node(Trie_node_t *trie, u32 ip);

#define IP_FMT	"%hhu.%hhu.%hhu.%hhu"
#define LE_IP_FMT_STR(ip) ((u8 *)&(ip))[3], \
						  ((u8 *)&(ip))[2], \
 						  ((u8 *)&(ip))[1], \
					      ((u8 *)&(ip))[0]

#define BE_IP_FMT_STR(ip) ((u8 *)&(ip))[0], \
						  ((u8 *)&(ip))[1], \
 						  ((u8 *)&(ip))[2], \
					      ((u8 *)&(ip))[3]

#define NET_IP_FMT_STR(ip)	BE_IP_FMT_STR(ip)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#	define HOST_IP_FMT_STR(ip)	LE_IP_FMT_STR(ip)
#elif __BYTE_ORDER == __BIG_ENDIAN
#	define HOST_IP_FMT_STR(ip)	BE_IP_FMT_STR(ip)
#endif
