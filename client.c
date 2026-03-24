#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(void) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    int connect_id = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connect_id < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    
    char buff[128] = "hello, server!";
    int send_srvr = send(sockfd, buff, 15, 0);    
    sleep(5);
    // close(sockfd);
    return 0;
}