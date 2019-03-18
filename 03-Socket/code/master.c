/* client application */
#include "mydef.h"

int main(int argc, char *argv[])
{
    int sock[WORKER_NUM];
    int i;
    struct sockaddr_in workers[WORKER_NUM];
     
    // create socket
    for(i = 0; i < WORKER_NUM; i ++)
    {
        if ((sock[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        {
            printf("create socket %d failed\n", i+1);
            return -1;
        }
        printf("socket %d created\n", i+1);
    }
     
    // get works' IP
    char workersIP[WORKER_NUM][15];
    FILE* workers_conf = fopen("./workers.conf", "r");

    for(i = 0; i < WORKER_NUM; i ++)
    {
        fscanf(workers_conf, "%s", workersIP[i]);
        printf("%s", workersIP[i]);
        workers[i].sin_addr.s_addr = inet_addr(workersIP[i]);
        workers[i].sin_family = AF_INET;
        workers[i].sin_port = htons(12345);

        // connect to workers
        if (connect(sock[i], (struct sockaddr *)&workers[i], sizeof(workers[i])) < 0) {
            printf("connect failed\n");
            return 1;
        }        
        printf("connected\n");
    }    

    //read txt file
    if(argc == 1){
        printf("No files to read.\n");
        return -1;
    } 
    FILE * fp = fopen(argv[1], "r");

    //count lines of txt file
    char buffer[2000];
    int line;
    for(line = 0; !feof(fp); line ++){
        fgets(buffer, sizeof(buffer), fp);
        memset(buffer, 0, sizeof(buffer));
    }

    // initialize
    send_msg msg;
    int workers_reply[2][26] = {0};
    int reply[26];
    int j;

    memset(msg.location, 0, sizeof(msg.location));
    strcat(msg.location, "./");
    strcat(msg.location, argv[1]);

    for(msg.start = 0, j = 0; j < WORKER_NUM; j++, msg.start += line/2)
    {
        msg.end = msg.start + line/2;

        // send data
        if (send(sock[j], (char *) & msg, sizeof(msg), 0) < 0) {
            printf("send failed\n");
            return -1;
        }

        // receive a reply from the workers
        int len = recv(sock[j], workers_reply[j], sizeof(workers_reply[j]), 0);
        if (len < 0) {
            printf("recv failed\n");
            break;
        }

        for(i = 0; i < 26; i++)
        	reply[i] += workers_reply[j][i];

    }

    // print results
    printf("Results:\n");
    for(i = 0; i < 26; i++)
        printf("%c: %d\n", i + 'a', reply[i]);

    // while(1) {
    //     // printf("enter message : ");
    //     // scanf("%s", message);
         
    //     // send some data
    //     if (send(sock, message, strlen(message), 0) < 0) {
    //         printf("send failed");
    //         return 1;
    //     }
         
    //     // receive a reply from the workers
	// 	int len = recv(sock, workers_reply, 2000, 0);
    //     if (len < 0) {
    //         printf("recv failed");
    //         break;
    //     }
	// 	workers_reply[len] = 0;
         
    //     printf("workers reply : ");
    //     printf("%s\n", workers_reply);
    // }
     
    close(sock[0]);
    close(sock[1]);
    return 0;
}
