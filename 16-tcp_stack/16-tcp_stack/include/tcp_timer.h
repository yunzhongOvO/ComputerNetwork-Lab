#ifndef __TCP_TIMER_H__
#define __TCP_TIMER_H__

#include "list.h"

#include <stddef.h>

struct tcp_timer {
	int type;	// now only support time-wait
	int timeout;	// in micro second
	int retrans_times;
	struct list_head list;
};

struct tcp_sock;
#define timewait_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, timewait))
#define retrans_timer_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, retrans_timer))

#define TCP_TIMER_SCAN_INTERVAL 100000
#define TCP_MSL			1000000
#define TCP_TIMEWAIT_TIMEOUT	(2 * TCP_MSL)
#define MAX_RETRANS 3
#define DEFAULT_RETRANS_TIME 200000
#define RETRANS_SCAN_TIMER 10000

// the thread that scans timer_list periodically
void *tcp_timer_thread(void *arg);

void *tcp_retrans_timer_thread(void *arg);

// add the timer of tcp sock to timer_list
void tcp_set_timewait_timer(struct tcp_sock *);

void tcp_set_retrans_timer(struct tcp_sock *tsk);
void tcp_init_retrans_timer(struct tcp_sock *tsk);
void tcp_unset_retrans_timer(struct tcp_sock *tsk);


#endif
