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
    
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);

    int connect_id = connect(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr));
    if (connect_id < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    
    char buff[128] = "hello, server!";
    int send_srvr = send(sockfd, buff, 15, 0);    
    sleep(10);
    close(sockfd);
    return 0;
}