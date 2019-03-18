#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define WORKER_NUM 2

typedef struct message
{
    //char fname;
    char location[100];    // master-> worker
    int start;
    int end;
} send_msg;