#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include <sqlite3.h>

#define MAX_CLIENTS 10
#define BUFF_SIZE 128
#define BUFF_DB_SIZE 512

typedef struct {
    int cl_fd;
    char buff[1024];
    int position;
} Clients;

int http_header(Clients client_list[MAX_CLIENTS]);

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

    Clients client_list[MAX_CLIENTS];
    
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
                }
                else {
                    rcv_srvr = recv(i, buff, BUFF_SIZE, 0);
                    if (rcv_srvr < 0) {
                        perror("receive");
                        continue;
                    }
                    else if (rcv_srvr > 0) {
                        buff[rcv_srvr] = '\0';
                        
                        int working_pos = client_list[i].position;
                        memcpy(&client_list[i].buff[working_pos], &buff, rcv_srvr);
                        client_list[i].position += rcv_srvr;

                        if (strstr(client_list[i].buff, "\r\n\r\n") != NULL) {
                            http_header(&client_list[i]);
                        }


                        sqlite3 *sql_db;
                        sqlite3_open("monitoring.db", &sql_db);
                        sqlite3_exec(sql_db, "CREATE TABLE IF NOT EXISTS metrics (hostname TEXT, timestamp TEXT)", NULL, NULL, NULL); 

                        char buff_send[BUFF_SIZE];                        
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        strftime(buff_send, sizeof(buff_send), "%d-%m-%Y %H:%M:%S", t);                           

                        char buff_db[BUFF_DB_SIZE];
                        snprintf(buff_db, BUFF_DB_SIZE, "INSERT INTO metrics (hostname, timestamp) VALUES ('%s', '%s')", buff, buff_send);
                        sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);

                        strncat(buff_send, ":log recorded", 14);
                        send(i, buff_send, sizeof(buff_send), 0);
                    }
                    else {
                        FD_CLR(i, &master_set);
                        close(i);
                    }                        
                }
            }
        }     
    }

    close(sockfd);
    
    return 0;
}

int http_header(Clients client_list[MAX_CLIENTS]) {
    char *method;
    char *path;
    char *version;

    char *p = client_list->buff;
    method = p;

    int i = 0;
    while (*p != '\n') {
        printf("%c\n", *p);
        if (*p == ' ') {
            *p = '\0';            
            if (i == 0) {
                path = p + 1;    
                i++;        
            }
            else version = p + 1;          
        }
        p++;
    }
    // printf("method: %s, path: %s, version: %s\n", method, path, version);

    return 0;
}