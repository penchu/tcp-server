#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

int main(void) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    // printf("serverid: %d\n", sockfd);
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return -1;
    } //should tell the OS to let you reuse the port even if it's in TIME_WAIT.

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        return -1;
    }

    while (1) {   
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_size = sizeof(peer_addr);
        printf("waiting for connection...\n");
        
        int clientfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size); 
        // int clientfd = accept(sockfd, NULL, NULL);
        printf("clientfd: %d\n", clientfd);
        if (clientfd < 0) {
            perror("accept");
            continue;
        }
        printf("connection accepted\n");
        
        char buff[128];
        int rcv_srvr = recv(clientfd, buff, 128, 0);
        buff[rcv_srvr] = '\0';
        printf("msg: %s\n", buff);

        if (close(clientfd) == -1) {
            perror("close");
            return -1;    
        }
    }

    close(sockfd);
    
    return 0;
}