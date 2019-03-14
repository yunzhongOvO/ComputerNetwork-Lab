/* client application */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define WORKER_NUM 3

struct message
{
    char fname;
    char location[50];    // master-> worker
    int start;
    int end;
} send_msg;


int main(int argc, char *argv[])
{
    int sock[WORKER_NUM];
    int i;
    struct sockaddr_in workers[WORKER_NUM];
    char message[1000], workers_reply[2000]; //TODO
     
    // create socket
    for(i = 0; i < WORKER_NUM; i ++){
        if ((sock[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("create socket %d failed\n", i+1);
            return -1;
        }
        printf("socket %d created\n", i+1);
    }
     
    // get works' IP
    char workersIP[WORKER_NUM][15];
    FILE* workers_conf = fopen("./workers.conf", "r");

    for(i = 0; i < WORKER_NUM; i ++){
        fscanf(workers_conf, "%s", workersIP[i]);
        printf("%s", workersIP[i]);
        workers.sin_addr.s_addr = inet_addr("10.0.0.1");
        workers.sin_family = AF_INET;
        workers.sin_port = htons(12345);

        // connect to workers
        if (connect(sock[i], (struct sockaddr *)&workers[i], sizeof(workers[i])) < 0) {
            perror("connect failed");
            return 1;
        }        
        printf("connected\n");
    }    

////-----------

    while(1) {
        // printf("enter message : ");
        // scanf("%s", message);
         
        // send some data
        if (send(sock, message, strlen(message), 0) < 0) {
            printf("send failed");
            return 1;
        }
         
        // receive a reply from the workers
		int len = recv(sock, workers_reply, 2000, 0);
        if (len < 0) {
            printf("recv failed");
            break;
        }
		workers_reply[len] = 0;
         
        printf("workers reply : ");
        printf("%s\n", workers_reply);
    }
     
    close(sock);
    return 0;
}
