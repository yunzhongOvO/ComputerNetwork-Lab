#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0){
		wake_up(tsk->wait_send);
	}
}

static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

void tcp_state_listen(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags & TCP_SYN) {
		struct tcp_sock *csk = alloc_tcp_sock();
		csk->sk_sip   = cb->daddr;
		csk->sk_sport = cb->dport;
		csk->sk_dip   = cb->saddr;
		csk->sk_dport = cb->sport;
		csk->iss = tcp_new_iss();
		csk->snd_una = csk->iss;
		csk->snd_nxt = csk->iss;
		csk->rcv_nxt = cb->seq_end;
		csk->parent = tsk;
		list_add_tail(&csk->list, &csk->listen_queue);
		tcp_set_state(csk, TCP_SYN_RECV);
		tcp_set_state(tsk, TCP_SYN_RECV);
		tcp_set_retrans_timer(csk);
		tcp_send_control_packet(csk, TCP_SYN | TCP_ACK);
		tcp_hash(csk);
	}
}

void tcp_state_syn_sent(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if( cb->flags & (TCP_SYN | TCP_ACK) ) {
		tsk->rcv_nxt = cb->seq_end;
		tsk->snd_una = cb->ack;
		snd_buf_rcv_ack(tsk, cb->ack);
		tcp_unset_retrans_timer(tsk);
		tcp_set_state(tsk, TCP_ESTABLISHED);
		tcp_send_control_packet(tsk, TCP_ACK);
		wake_up(tsk->wait_connect);
	}
	// else
	// 	tcp_send_reset(cb);
}

void tcp_state_syn_recv(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	
	if(cb->flags & TCP_ACK) {
		struct tcp_sock *csk = tsk, *parent_tsk = csk->parent;
		tcp_sock_accept_enqueue(csk);
		csk->rcv_nxt = cb->seq_end;
		tsk->snd_una = cb->ack;
		snd_buf_rcv_ack(tsk, cb->ack);
		tcp_unset_retrans_timer(tsk);
		tcp_set_state(parent_tsk, TCP_ESTABLISHED);
		tcp_set_state(csk, TCP_ESTABLISHED);
		wake_up(parent_tsk->wait_accept);
	}
}

void tcp_recv_data(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(less_than_32b(cb->seq, tsk->rcv_nxt)){
		return;
	}
	add_ofo_buf(tsk, cb, packet);
	remove_ofo_buf(tsk); 
	tsk->snd_una = (greater_than_32b(cb->ack, tsk->snd_una))?cb->ack :tsk->snd_una;
	tcp_set_retrans_timer(tsk);
	tcp_send_data(tsk, "yes",sizeof("yes"));
}

void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (tsk->state == TCP_CLOSED) {
		// tcp_send_reset(cb);
		return;
	}
	if (tsk->state == TCP_LISTEN) {
		
		tcp_state_listen(tsk, cb, packet);
		return;
	}
	if (tsk->state == TCP_SYN_SENT) {
		tcp_state_syn_sent(tsk, cb, packet);
		return;
	}
	
	if (cb->flags & TCP_RST ) {
		tcp_sock_close(tsk);
		free_tcp_sock(tsk);
		return;
	}

	if (cb->flags & TCP_SYN){
		// tcp_send_reset(cb);
		tcp_sock_close(tsk);
		return;
	}
	// if (!(cb->flags & TCP_ACK)){ 
	// 	// tcp_send_reset(cb);
	// 	return;
	// }
	if (cb->flags & TCP_ACK) {
		if (tsk->state == TCP_SYN_RECV) {
			tcp_state_syn_recv(tsk, cb, packet); 
			return;
		}
		else if (tsk->state == TCP_ESTABLISHED && !(cb->flags & TCP_FIN) ) {
			if(!cb->pl_len || !strcmp(cb->payload,"yes")){ 
				tsk->snd_una = cb->ack;
				tsk->rcv_nxt = cb->seq + 1;
				tcp_update_window_safe(tsk, cb);
				snd_buf_rcv_ack(tsk, cb->ack);
				tcp_init_retrans_timer(tsk);
				wake_up(tsk->wait_send);
				return;
			}
			else{
				tcp_recv_data(tsk, cb, packet);
				return;
			}
		}
		else if(!(cb->flags & TCP_FIN)){
			switch(tsk->state){
				case TCP_FIN_WAIT_1:
					snd_buf_rcv_ack(tsk, cb->ack);
					tcp_unset_retrans_timer(tsk);
					tcp_set_state(tsk, TCP_FIN_WAIT_2);
					return;
				case TCP_CLOSING:
					snd_buf_rcv_ack(tsk, cb->ack);
					tcp_unset_retrans_timer(tsk);
					tcp_set_state(tsk, TCP_TIME_WAIT);
					tcp_set_timewait_timer(tsk);
					tcp_unhash(tsk);
					return;
				case TCP_LAST_ACK:
					snd_buf_rcv_ack(tsk, cb->ack);
					tcp_unset_retrans_timer(tsk);
					tcp_set_state(tsk, TCP_CLOSED);
					return;
				default:
					break;
			}
		}
	}
	if(cb->flags & TCP_FIN){
		switch(tsk->state){
			case TCP_ESTABLISHED:
				tsk->rcv_nxt = cb->seq+1;
				wait_exit(tsk->wait_recv);
				wait_exit(tsk->wait_send);
				tcp_set_state(tsk, TCP_CLOSE_WAIT);
				tcp_send_control_packet(tsk, TCP_ACK);
				return;
			case TCP_FIN_WAIT_1:
				tcp_set_state(tsk, TCP_CLOSING);
				tcp_send_control_packet(tsk, TCP_ACK);
				return;
			case TCP_FIN_WAIT_2:
				tsk->rcv_nxt = cb->seq+1;
				tcp_set_state(tsk, TCP_TIME_WAIT);
				tcp_set_timewait_timer(tsk);
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_unhash(tsk);
				return;
			default:
				break;
		}
	}

	tcp_send_control_packet(tsk, TCP_ACK);
	return;
}
