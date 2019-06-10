# 网络传输机制实验二报告 
- - -
## 实验内容

#### 实现数据传输

- 如何将数据封装到数据包并发送

- 收到数据和ACK时的相应处理

#### 实现流量控制

- 通过调整recv_window来表达自己的接收能力

#### 实现tcp_sock相关函数

- 类似socket函数，能够收发数据

## 实验步骤

#### 实现tcp_sock_read函数

- 通过`read_ring_buffer`函数读取缓存区数据，如果缓存区已满，阻塞

- 返回值

  - 0表示读到流结尾，对方关闭连接
  - -1表示出现错误
  - 正值表示读取的数据长度

  ```c
  int tcp_sock_read(struct tcp_sock *tsk, char *buf, int len)
  {
  	if (ring_buffer_empty(tsk->rcv_buf)) {
  		if (sleep_on(tsk->wait_recv) < 0)
  			return -1;
  	}
  	pthread_mutex_lock(&tsk->rcv_buf_lock);
  	int rlen = read_ring_buffer(tsk->rcv_buf, buf, len);
  	pthread_mutex_unlock(&tsk->rcv_buf_lock);
  
  	return rlen;
  }
  ```

#### 实现tcp_sock_write函数

- 注意数据包的大小是`ETH_FRAME_LEN` 和`len` 加一系列包头二值的较小值

- 如果当前发送窗口大小不够发送，阻塞write函数，等待发送窗口更新

  ```c
  int tcp_sock_write(struct tcp_sock *tsk, char *buf, int len)
  {
  	if (!tsk->snd_wnd)  sleep_on(tsk->wait_send);
  	int DEFAULT_SIZE = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE;
  	int plen = min(len + DEFAULT_SIZE, ETH_FRAME_LEN);
  	char *pkt = malloc(plen);
  	char *buffer = pkt + DEFAULT_SIZE;
  	memcpy(buffer, buf, plen - DEFAULT_SIZE);
  	if (tsk->snd_wnd >= plen) {
  		tcp_send_packet(tsk, pkt, plen);
  		return 1;
  	}
  	return 0;
  }
  ```

### 修改tcp_process函数

- 在`TCP_SYN_RECV`状态之后用`tcp_update_window_safe`函数更新发送窗口大小
- 在`TCP_ESTABLISHED`状态唤醒`tsk->wait_recv`

## 实验结果及分析

- 运行命令

  ```shell
  terminal # sudo python tcp_topo.py
  mininet # xterm h1 h2
  h1 # ./tcp_stack server 10001
  h2 # ./tcp_stack client 10.0.0.1 10001
  ```

- 运行结果

  ![](pic/1.PNG)

- 使用`tcp_stack.py`替换其中任意一端，对端都能正确收发数据

  ![](pic/2.PNG)
