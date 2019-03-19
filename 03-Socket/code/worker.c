/* server application */

#include "mydef.h"

void record(char * s, int t[])
{
    int i, j;
    // convert uppercase to lowercase
    for(i = 0; i < strlen(s); i ++)
        s[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] - 'A' + 'a' : s[i];
    
    // record numbers of 26 letters
    for(i = 0; i < strlen(s); i ++)
    {
        if(s[i] >= 'a' && s[i] <= 'z') 
        {
            j = s[i] - 'a';
            ++ t[j];
        }

        // j = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 'a' : -1;
        // if (j == -1)
        //     ++ t[j];
    }
    return;
}

// void Atoa(char* message,int stat[]){
//     int key;
//     low(message);
//     for(int i=0; i<strlen(message) ;++i){
//         key = (message[i]>='a' && message[i]<='z')? message[i]-'a' : -1;
//         if(key != -1)
//             ++stat[key];
//     }
//     return;
// }

int main(int argc, const char *argv[])
{
    int s, cs;
    struct sockaddr_in server, client;
     
    // create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create socket failed\n");
		return -1;
    }
    printf("socket created\n");
     
    // prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(12345);
     
    // bind
    if (bind(s,(struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("bind failed\n");
        return -1;
    }
    printf("bind done\n");
     
    // listen
    listen(s, 3);
    printf("waiting for incoming connections...\n");
     
    // accept connection from an incoming client
    int c = sizeof(struct sockaddr_in);
    if ((cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
        printf("accept failed\n");
        return -1;
    }
    printf("connection accepted\n");
     
	int msg_len = 0;
    send_msg msg;

    // receive a message from client
    while ((msg_len = recv(cs, (char *)&msg, sizeof(msg), 0)) > 0) {
        // send the message back to client
        FILE * fp = fopen((char *)msg.location, "r");
        int reply[26] = {0};
        char buffer[2000] = {0};
        int i;

        for(i = 0; i < msg.start; i ++)
            fgets(buffer, sizeof(buffer), fp);

        for(i = msg.start; i < msg.end; i ++){
            fgets(buffer, sizeof(buffer), fp);
            record(buffer, reply);
        }

        write(cs, (char *)reply, sizeof(reply));
        memset(reply, 0, sizeof(reply));
    }
     
    if (msg_len == 0) {
        printf("client disconnected\n");
    }
    else { // msg_len < 0
        printf("recv failed\n");
		return -1;
    }
     
    return 0;
}
