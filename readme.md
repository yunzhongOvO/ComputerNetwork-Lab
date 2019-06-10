# [UCAS Course] 

# Computer Network Experiment Lab

###  02-Mininet [互联网协议实验]

> 用wget下载www.baidu.com页面，用wireshark抓包
>
> 调研说明wireshark抓到的几种协议：ARP, DNS, TCP, HTTP
>
> 调研解释h1下载baidu页面的整个过程：几种协议的运行机制

### 03-Socket[应用编程实验]

> 使用BSD Socket API接口，实现一个基于socket的分布式字符统计程序

### 04-Broadcast[广播网络实验]

> 实现广播网络
>
> 验证广播网络的效率

### 05-Switching[交换机学习实验]

> 实现一个具有转发表学习功能的交换机，使得连接到同一交换机的主机可以相互通信

### 06-Stp[生成树机制实验]

> 生成树机制：通过禁止(block) 设备的相关端口，在有环路的网络中构造出一个总体开销最小的树状拓扑，使得网络在连通的前提下，避免广播风暴



------



### 08-Router[路由器转发实验]

> 给定网络拓扑以及节点的路由表配置，实现路由器的转发功能，使得各节点之间能够连通并传送数据

### 09-Nat[网络地址转换实验]

> 维护NAT连接映射表，支持映射的添加、查找、更新和老化操作
>
> 处理到达的数据包

### 10-Lookup[高效IP路由查找实验]

> 实现最基本的前缀树查找、多比特前缀树、poptrie

### 11-mospf[网络路由实验]

> 实现路由器生成和处理mOSPF Hello/LSU消息的相关操作，构建一致性链路状态数据库
>
> mOSPF indicates mini-OSPF

### 12-BufferBloat[数据包队列管理实验]

> h1(发送方)在对h2进行iperf的同时，测量h1的拥塞窗口值、r1-eth1的队列长度、h1与h2间的往返延迟
>
> 改变r1-eth1的队列大小，考察其对iperf吞吐率和ping延迟的影响
>
> 使用CoDel、Red、Tail Drop三种方法解决BufferBloat问题，重现带宽对ping延迟的影响



------



### 14-tcp_stack[网络传输机制实验报告 ]

> 实现TCP数据包处理
>
> 实现tcp_sock连接管理函数

### 15-tcp_stack[网络传输机制实验报告 ]

> 实现tcp_sock相关函数，能够收发数据