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
    // printf("sin_addr raw: 0x%x\n", server_addr.sin_addr.s_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    // if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
    //     perror("inet_pton failed");
    //     return -1;
    // }    

    printf("Connecting to %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
    printf("INADDR_LOOPBACK raw: 0x%x\n", INADDR_LOOPBACK);

    int connect_id = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (connect_id < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    
    char buff[128] = "hello, server!";
    int send_srvr = send(sockfd, buff, 15, 0);    
    // printf("send_srvr: %d\n", send_srvr);
    // printf("Connecting to: %s\n", inet_ntoa(server_addr.sin_addr));
    sleep(5);
    // close(sockfd);
    return 0;
}