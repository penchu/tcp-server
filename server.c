#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

int main(void) {
    int sockfd;
    if (sockfd = socket(AF_INET, SOCK_STREAM, 0) < 0) {
        perror("socket");
        return -1;
    }

    int bind_return;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    if (bind_return = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return -1;
    }
}