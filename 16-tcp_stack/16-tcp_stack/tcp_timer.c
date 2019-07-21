#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <unistd.h>
#include <stdio.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *p, *q;
	list_for_each_entry_safe(p, q, &timer_list, list) {
		p->timeout -= TCP_TIMER_SCAN_INTERVAL;
		if (p->timeout <= 0) {
			list_delete_entry(&p->list);

			// only support time wait now
			tsk = timewait_to_tcp_sock(p);
			list_delete_entry(&tsk->retrans_timer.list);
			if (! tsk->parent)
				tcp_bind_unhash(tsk);
			tcp_set_state(tsk, TCP_CLOSED);
			free_tcp_sock(tsk);
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->timewait;

	timer->type = 0;
	timer->timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&timer->list, &timer_list);
	tsk->ref_cnt ++;
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}


static struct list_head retrans_timer_list;

void tcp_scan_retrans_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *p, *q;
	list_for_each_entry_safe(p, q, &retrans_timer_list, list) {
		p->timeout -= RETRANS_SCAN_TIMER;
		tsk = retrans_timer_to_tcp_sock(p);
		if(p->timeout <= 0){
			if(p->retrans_times>=MAX_RETRANS){
				list_delete_entry(&p->list);
				if (! tsk->parent)
					tcp_bind_unhash(tsk);
				wait_exit(tsk->wait_connect);
				wait_exit(tsk->wait_accept);
				wait_exit(tsk->wait_recv);
				wait_exit(tsk->wait_send);
				
				tcp_set_state(tsk, TCP_CLOSED);
				free_tcp_sock(tsk);
				free_snd_buf();
				exit(0);
			}
			else{
				p->retrans_times += 1;
				p->timeout = DEFAULT_RETRANS_TIME * (2 << p->retrans_times);
				retrans_snd_buf(tsk);
			}
		}
	}
}

void tcp_set_retrans_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;

	timer->type = 0;
	timer->timeout = DEFAULT_RETRANS_TIME;
	timer->retrans_times = 0;
	init_list_head(&timer->list);
	list_add_tail(&timer->list, &retrans_timer_list);
	tsk->ref_cnt ++;
}

void tcp_init_retrans_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;
	timer->type = 0;
	timer->timeout = DEFAULT_RETRANS_TIME;
	timer->retrans_times = 0;
}

void tcp_unset_retrans_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;
	list_delete_entry(&timer->list);
	free_tcp_sock(tsk);
}


void *tcp_retrans_timer_thread(void *arg){
	init_list_head(&retrans_timer_list);
	while(1){
		usleep(RETRANS_SCAN_TIMER);
		tcp_scan_retrans_timer_list();
	}
}






