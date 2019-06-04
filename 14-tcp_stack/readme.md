# 14-网络传输机制实验报告
- - -
## 实验内容

- 实现TCP数据包处理

  - 如何建立连接、关闭连接、处理异常情况
  
- 实现tcp_sock连接管理函数

  - 类似于socket函数，能够绑定和监听端口，建立和关闭连接

## 实验步骤

### 实现TCP接收数据包(tcp_process)

按照如下原理图实现状态转换：

![](./pic/tcp.PNG)

- 如果数据包的RESET置1，则转换为`TCP_CLOSED`状态，并释放established_list

  ```c
  	if (cb->flags & TCP_RST) {
  		tcp_set_state(tsk, TCP_CLOSED);
          tcp_unhash(tsk);
  		free(tsk);
  		return;
  	}
  ```

- 在`TCP_LISENT`状态记录对端连续确认的最大序列号`nu32 snd_una`和本端发送的最大序列号`nu32 snd_nxt`

  ```c
  	if (tsk->state != TCP_CLOSED && tsk->state != TCP_LISTEN) {
  		tsk->snd_una = cb->ack;
  		tsk->rcv_nxt = cb->seq_end;
  		if (!is_tcp_seq_valid(tsk, cb))
  			return;
  	}
  ```

- 用`switch-case`语句实现状态转换

  ```c
  	switch (tsk->state)
  	{
  		case TCP_CLOSED:
  			tcp_send_reset(cb);
  			tcp_set_state(tsk, TCP_CLOSED);
  			break;
  		case TCP_LISTEN:
  			if (cb->flags & TCP_SYN) { 
                  // set up child tcp_sock, omitted here
  				tcp_set_state(csk, TCP_SYN_RECV);
  				tcp_hash(csk);
  				tcp_send_control_packet(csk, TCP_SYN | TCP_ACK);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  		case TCP_SYN_RECV:
  			if (cb->flags & TCP_ACK) {
  				tcp_set_state(tsk, TCP_ESTABLISHED);
  				tcp_sock_listen_dequeue(tsk);
  				tcp_sock_accept_enqueue(tsk);
  				wake_up(tsk->parent->wait_accept);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  		case TCP_SYN_SENT:
  			if (cb->flags & (TCP_SYN | TCP_ACK)) {
  				tcp_set_state(tsk, TCP_ESTABLISHED);
  				wake_up(tsk->wait_connect);
  				tcp_send_control_packet(tsk, TCP_ACK);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  		case TCP_ESTABLISHED:
  			if (cb->flags & TCP_FIN) {
  				tcp_set_state(tsk, TCP_CLOSE_WAIT);
          		tcp_send_control_packet(tsk, TCP_ACK);
  			} else if (cb->flags & TCP_ACK) {
      			tcp_send_control_packet(tsk, TCP_ACK);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  
  		case TCP_CLOSE_WAIT:
  			tcp_send_reset(cb);
  			tcp_set_state(tsk, TCP_CLOSED);
  			break;
  		case TCP_LAST_ACK:
  			if ((cb->flags & TCP_ACK) && cb->ack == tsk->snd_nxt) {
  				tcp_unhash(tsk);
  				tcp_bind_unhash(tsk);
  			} else {
  				tcp_send_reset(cb);
  			}
  			tcp_set_state(tsk, TCP_CLOSED);
  			break;
  		case TCP_FIN_WAIT_1: 
  			if ((cb->flags & TCP_ACK) && cb->ack == tsk->snd_nxt) {
  				tcp_set_state(tsk, TCP_FIN_WAIT_2);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  		case TCP_FIN_WAIT_2: 
  			if ((cb->flags & (TCP_ACK | TCP_FIN)) && cb->ack == tsk->snd_nxt) {
  				tcp_set_state(tsk, TCP_TIME_WAIT);
  				tcp_set_timewait_timer(tsk);
  				tcp_send_control_packet(tsk, TCP_ACK);
  			} else {
  				tcp_send_reset(cb);
  				tcp_set_state(tsk, TCP_CLOSED);
  			}
  			break;
  		default:
  			tcp_send_reset(cb);
  			tcp_set_state(tsk, TCP_CLOSED);
  			break;
  	}
  ```

### 实现socket原语

#### tcp_sock_listen

- 设置backlog，将TCP状态转换为`TCP_LISTEN`，把`tcp sock`放入`listen_table`

  ```c
  int tcp_sock_listen(struct tcp_sock *tsk, int backlog)
  {
  	if (backlog > TCP_MAX_BACKLOG)
  		return -1;
  	tsk->backlog = backlog;
  	tcp_set_state(tsk, TCP_LISTEN);
  	return tcp_hash(tsk);
  }
  ```

#### tcp_sock_connect

- 初始化四元组`sip`, `sport`, `dip`, `dport`

- 把`tcp sock`放入`bind_table`

- 转换为`TCP_SYN_SENT `状态，睡眠等待SYN包

  ```c
  int tcp_sock_connect(struct tcp_sock *tsk, struct sock_addr *skaddr)
  {
  	u16 port = 0;
  	if (!(port = tcp_get_port()))
  		return -1;	
  	tsk->sk_sport = port;
  	tsk->sk_sip = ((iface_info_t *)(instance->iface_list.next))->ip;
  	tsk->sk_dip = ntohl(skaddr->ip);
  	tsk->sk_dport = ntohs(skaddr->port);
  	tcp_bind_hash(tsk);
  	tsk->iss = tcp_new_iss();
  	tsk->snd_nxt = tsk->iss;
  	tcp_send_control_packet(tsk, TCP_SYN);
  	tcp_set_state(tsk, TCP_SYN_SENT);
  	tcp_hash(tsk);
  	sleep_on(tsk->wait_connect);
  	return 0;
  }
  ```

#### tcp_sock_accept

- 如果`accept_queue`不为空，则取出一个sock并接受；否则，将connect请求阻塞等待

  ```c
  struct tcp_sock *tcp_sock_accept(struct tcp_sock *tsk)
  {
  	if (list_empty(&tsk->accept_queue))
  		sleep_on(tsk->wait_accept);
  	struct tcp_sock *acc = tcp_sock_accept_dequeue(tsk);
  	tcp_set_state(acc, TCP_ESTABLISHED);
  	return acc;
  }
  ```

#### tcp_sock_close

- 对于服务器，将状态由`TCP_CLOSE_WAIT`转换为`TCP_LAST_ACK`

- 对于客户端，将状态由`TCP_ESTABLISHED`转换为`TCP_FIN_WAIT1`

  ```c
  void tcp_sock_close(struct tcp_sock *tsk)
  {
  	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
  	switch (tsk->state)
  	{
  		case TCP_LISTEN:
  			tcp_sock_clear_listen_queue(tsk);
  			if (!tsk->parent)
  				tcp_bind_unhash(tsk);
  			free_tcp_sock(tsk);
  			tcp_set_state(tsk, TCP_CLOSED);
  			break;
  		case TCP_ESTABLISHED:
  			tcp_send_control_packet(tsk, TCP_FIN | TCP_ACK);
  			tcp_set_state(tsk, TCP_FIN_WAIT_1);
  			break;
  		case TCP_CLOSE_WAIT:
  			tcp_send_control_packet(tsk, TCP_FIN | TCP_ACK);
  			tcp_set_state(tsk, TCP_LAST_ACK);
  			break;
  		default:
  			break;
  	}
  }
  ```

### 实现TCP定时器

#### tcp_scan_timer_list

- 扫描`timer_list`，释放停留超过 2 * MSL 时间的`tcp sock`

  ```c
  void tcp_scan_timer_list()
  {
  	struct tcp_sock *tsk;
  	struct tcp_timer *p, *q;
  	list_for_each_entry_safe(p, q, &timer_list, list) {
  		p->timeout -= TCP_TIMER_SCAN_INTERVAL;
  		if (p->timeout <= 0) {
  			list_delete_entry(&p->list);
  			tsk = timewait_to_tcp_sock(p);
  			// decrease ref_cnt
  			if (!tsk->parent)	tcp_bind_unhash(tsk);
  			tcp_unhash(tsk);
  			tcp_set_state(tsk, TCP_CLOSED);
  			free_tcp_sock(tsk);
  		}
  	}
  }
  ```

#### tcp_set_timewait_timer

- 初始化`tcp sock`的计时器：把`tcp sock`的计时器加入`timer_list`

  ```c
    void tcp_set_timewait_timer(struct tcp_sock *tsk)
    {
    	struct tcp_timer *t = &tsk->timewait;
    	t->type = 0;
    	t->timeout = TCP_TIMEWAIT_TIMEOUT;
    	list_add_tail(&t->list, &timer_list);
    	tsk->ref_cnt += 1;
    }
  ```

## 实验结果及分析

- **运行给定网络拓扑**(`tcp_topo.py`)

- **在h1上运行TCP协议栈的服务器模式**  (`./tcp_stack server 10001`)

- **在h2上运行TCP协议栈的客户端模式** (`./tcp_stack client 10.0.0.1 10001`)

  ![](./pic/xterm.PNG)

- **在一端用tcp_stack.py替换tcp_stack执行，测试另一端**

  - 结果相同，说明TCP栈能够正常工作

- **通过wireshark抓包来来验证建立和关闭连接的正确性**

  ![](./pic/wireshark.PNG)

  由图像知，TCP的三次握手与四次挥手都正确完成。

## 实验总结

- TCP状态转换时，需要在`TCP_LISTEN`状态后再经过一次握手来检查收到的seq是否有效，否则可能会会因为seq不合理而导致建立连接失败。
- `tcp_sock_accept`函数中，如果`accept_queue`不为空，则不进入阻塞状态，直接从队列中弹出一个`tcp sock`。