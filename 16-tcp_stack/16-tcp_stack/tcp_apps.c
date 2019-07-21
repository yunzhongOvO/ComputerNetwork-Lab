#include "tcp_sock.h"

#include "log.h"

#include <unistd.h>
#include<stdio.h>
#define MAX_BUF 4*1024*1024 // 4MB

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	FILE *f = fopen("./server-output.dat","w+");
	char* rbuf = (char*)malloc(MAX_BUF);
	int rlen = 0;
	int total_recv = 0;
	while(1){
		memset(rbuf, 0, MAX_BUF);
		rlen = tcp_sock_read(csk, rbuf, MAX_BUF);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			fprintf(f, "%s", rbuf);
			total_recv += rlen;
			fprintf(stdout, "[recv] %d KB\t[total] %d KB\n", rlen/1024, total_recv/1024);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}
	fclose(f);

	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	FILE *f = fopen("./client-input.dat","r");
	char* buf = (char *)malloc(MAX_BUF);
	int len = fread(buf, sizeof(char), MAX_BUF, f);
	fclose(f);

	if(tcp_sock_write(tsk, buf, len) < 0){
		fprintf(stdout,"[tcp_sock_write_error]");
	}

	tcp_sock_close(tsk);

	return NULL;
}
