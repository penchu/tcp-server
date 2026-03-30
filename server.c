#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define MAX_CLIENTS 10

int main(void) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    
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

    int client_fds[MAX_CLIENTS];
    memset(client_fds, -1, sizeof(client_fds));  

    int sel_val;
    fd_set master_set;
    struct timeval tv;
    FD_ZERO(&master_set);
    FD_SET(sockfd, &master_set);

    int max_fd = sockfd;
    int clientfd;
    char buff[128];
    int rcv_srvr;    

    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);   
    
    while (1) {        
        fd_set read_set = master_set;
        sel_val = select((max_fd+1), &read_set, NULL, NULL, NULL);

        if (sel_val < 0) {
            perror("select");
            continue;
        }
        
        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == sockfd) {                 
                    clientfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size); 
                    if (clientfd < 0) {
                        perror("accept");
                        continue;
                    }  
                    if (clientfd > max_fd) max_fd = clientfd;   
                    FD_SET(clientfd, &master_set);   
                    printf("client: %d\n", clientfd);
                }
                else {
                    rcv_srvr = recv(i, buff, 128, 0);
                    if (rcv_srvr < 0) {
                        perror("receive");
                        continue;
                    }
                    else if (rcv_srvr > 0) {
                        buff[rcv_srvr] = '\0';
                        printf("msg: %s\n", buff);

                        char buff_send[128] = "HELLO!";
                        send(i, buff_send, 7, 0);

                    }
                    else {
                        FD_CLR(i, &master_set);
                        close(i);
                        printf("test\n");
                    }                        
                }
            }
        }     
    }

    close(sockfd);
    
    return 0;
}