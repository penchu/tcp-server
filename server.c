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
#include <uuid/uuid.h>
#include <sodium.h>
#include <jwt.h>

#define MAX_CLIENTS 10
#define BUFF_SIZE 128
#define BUFF_DB_SIZE 512

// time_t now;

typedef struct {
    int cl_fd;
    char buff[1024];
    int position;
    char *method;
    char *path;
    char *version;
    char *body;
    char *token;
} Clients;

typedef struct {
    time_t now;
    int requests;
} Server;

typedef struct {
    int position;
    char buff[BUFF_SIZE*40];
} Callback;

typedef struct {
    char status_code[BUFF_SIZE];
    char body[BUFF_SIZE*40];
    char type[BUFF_SIZE];
} Response;

Server server;

int server_init(int *sockfd);
int server_run(int *sockfd);
int handle_new_client(int *sockfd, int *max_fd, fd_set *master_set);
int handle_client_data(Clients *client, char *buff, int *rcv_srvr);
int http_header_parse(Clients *client);
int db_store(char *buff, char *buff_send);
int handle_request(Clients *client);
int handle_health(Clients *client, Response *r);
int handle_metrics(Clients *client, Response *r);
int handle_users(sqlite3 *sql_db, Clients *client, Response *r);
int db_users(sqlite3 *sql_db, Clients *client, Response *r);
int callback_func(void *callback_data, int num_columns, char** values, char** col_names);
int GET_response(Clients *client, Response *r);
int DEL_users(sqlite3 *sql_db, Clients *client, Response *r);
int UPDATE_users(sqlite3 *sql_db, Clients *client, Response *r);
char *hashing_passwd(char *passwd, Response *r);
int handle_login(sqlite3 *sql_db, Clients *client, Response *r);
int JWT_Token(const char *user_id, Response *r);

int main(void) {
    int sockfd;
    
    server_init(&sockfd);
    server_run(&sockfd);   

    close(sockfd);
    
    return 0;
}

int server_init(int *sockfd) {
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return -1;
    } //should tell the OS to let you reuse the port even if it's in TIME_WAIT.

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (bind(*sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(*sockfd);
        return -1;
    }
    
    if (listen(*sockfd, 5) < 0) {
        perror("listen");
        return -1;
    }
       
    server.now = time(NULL);
    
    return 0;
}

int server_run(int *sockfd) {
    int client_fds[MAX_CLIENTS];
    memset(client_fds, -1, sizeof(client_fds));  

    int sel_val;
    fd_set master_set;
    struct timeval tv;
    FD_ZERO(&master_set);
    FD_SET(*sockfd, &master_set);

    int max_fd = *sockfd;
    char buff[BUFF_SIZE];
    int rcv_srvr;    

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
                if (i == *sockfd) {  
                    handle_new_client(sockfd, &max_fd, &master_set);
                }
                else {                    
                    rcv_srvr = recv(i, buff, BUFF_SIZE, 0);
                    if (rcv_srvr < 0) {
                        perror("receive");
                        continue;
                    }
                    else if (rcv_srvr > 0) {
                        buff[rcv_srvr] = '\0';
                        client_list[i].cl_fd = i;
                        handle_client_data(&client_list[i], buff, &rcv_srvr);
                
                        // char buff_send[BUFF_SIZE];
                        // time_t now = time(NULL);
                        // struct tm *t = localtime(&now);
                        // strftime(buff_send, sizeof(buff_send), "%d-%m-%Y %H:%M:%S", t);
                        // strncat(buff_send, ":log recorded", 14);
                        // send(i, buff_send, sizeof(buff_send), 0);
                        // db_store(buff, buff_send);
                    }
                    else {
                        // printf("fd clr\n");
                        FD_CLR(i, &master_set);
                        memset(&client_list[i], 0, sizeof(Clients));
                        close(i);
                    }                        
                }
            }
        }     
    }
    return 0;
}

int handle_new_client(int *sockfd, int *max_fd, fd_set *master_set) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr); 

    int clientfd = accept(*sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size); 
    if (clientfd < 0) {
        perror("accept");
        return -1;
    }  
    if (clientfd > *max_fd) *max_fd = clientfd;   
    FD_SET(clientfd, master_set); 
    return 0;
}

int handle_client_data(Clients *client, char *buff, int *rcv_srvr) {
    int working_pos = client->position;
    
    memcpy(&client->buff[working_pos], buff, *rcv_srvr);
    client->position += *rcv_srvr;

    if (strstr(client->buff, "\r\n\r\n") != NULL) {
        http_header_parse(client);
    }

    return 0;
}

int http_header_parse(Clients *client) {

    // printf("%s\n", client->buff);    
    // if ((client->token = strstr(client->buff, "Authorization: Bearer "))) {
    //     client->token += strlen("Authorization: Bearer ");
    // }   
    // client->token[strcspn(client->token, "\r")] = '\0';    
    // printf("%s\n", client->token);
    // char *r = strchr(client->token, '\r');
    // *r = '\0';
    // printf("%s\n", client->token);
    // exit(0);
    
    // int cont_len;
    // char *length;
    // char *endptr;
    // if ((length = strstr(client->buff, "Content-Length")) != NULL) {        
    //     length = strchr(length, ':') + 1;
    // }  
    // if (length) strtoul(length, &endptr, 0);
  
    char *body_str = strstr(client->buff, "\r\n\r\n");   
    client->body = body_str + 4;    

    if ((client->token = strstr(client->buff, "Authorization: Bearer "))) {
        client->token += strlen("Authorization: Bearer ");
    }   
    client->token[strcspn(client->token, "\r")] = '\0'; 
    
    char *p = client->buff;
    client->method = p;

    int i = 0;
    while (*p) {
        if (*p == ' ') {
            *p = '\0';            
            if (i == 0) {
                client->path = p + 1;    
                i++;        
            }
            else {
                client->version = p + 1;   
                break;    
            }   
        }
        p++;
    }
    client->version[strcspn(client->version, "\r")] = '\0';
    // printf("method: %s, path: %s, version: %s\n", client->method, client->path, client->version);    
    
    handle_request(client);
    
    return 0;
}

int db_store(char *buff, char *buff_send) {
    sqlite3 *sql_db;
    sqlite3_open("monitoring.db", &sql_db);
    sqlite3_exec(sql_db, "CREATE TABLE IF NOT EXISTS metrics (hostname TEXT, timestamp TEXT)", NULL, NULL, NULL); 
    
    // char buff_send[BUFF_SIZE];                        
    // time_t now = time(NULL);
    // struct tm *t = localtime(&now);
    // strftime(buff_send, sizeof(buff_send), "%d-%m-%Y %H:%M:%S", t);                           
    
    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, BUFF_DB_SIZE, "INSERT INTO metrics (hostname, timestamp) VALUES ('%s', '%s')", buff, buff_send);
    sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
    
    return 0;
}

int handle_request(Clients *client) {
    // printf("method: %s, path: %s, version: %s, body: %s\n", client->method, client->path, client->version, client->body);
    Response response;
    memset(&response, 0, sizeof(Response));
    
    sqlite3 *sql_db;
    sqlite3_open("monit_users.db", &sql_db);
    sqlite3_exec(sql_db, "CREATE TABLE IF NOT EXISTS users (UUID TEXT PRIMARY KEY, username TEXT, password TEXT, timestamp TEXT)", 
                NULL, NULL, NULL);  

    if (strcmp("/health", client->path) == 0) {
        handle_health(client, &response);
    }
    else if (strcmp("/metrics", client->path) == 0) {
        handle_metrics(client, &response);
    }
    else if (strstr(client->path, "/users") != NULL) {
        handle_users(sql_db, client, &response);
    }
    else if (strstr(client->path, "/login") != NULL) {
        handle_login(sql_db, client, &response);
    }
    
    char buff_send[BUFF_SIZE*40];
    memset(buff_send, 0, sizeof(buff_send));
    int position;
    int len = strlen(response.body);
    position = snprintf(buff_send, sizeof(buff_send), "HTTP/1.1 %s\r\n", response.status_code);
    if (response.body[0] != '\0') {
        position += snprintf(buff_send + position, sizeof(buff_send), "Content-Type: %s\r\n", response.type);
        position += snprintf(buff_send + position, sizeof(buff_send), "Content-Length: %d\r\n", len);
        position += snprintf(buff_send + position, sizeof(buff_send), "%s", "\r\n");
        position += snprintf(buff_send + position, sizeof(buff_send), "%s", response.body);
    }
    else position += snprintf(buff_send + position, sizeof(buff_send), "%s", "\r\n");
    send(client->cl_fd, buff_send, position, 0);

    return 0;
}

int handle_health(Clients *client, Response *r) {
    server.requests++;

    snprintf(r->body, sizeof(r->body), "%s", "{\"status\": \"ok\"}\n");
    snprintf(r->status_code, sizeof(r->status_code), "%s", "200 OK");
    snprintf(r->type, sizeof(r->type), "%s", "application/json");

    return 0;
}

int handle_metrics(Clients *client, Response *r) {
    server.requests++;
    time_t now_2 = time(NULL);
    int time_diff = difftime(now_2, server.now);

    snprintf(r->body, sizeof(r->body), "{\"uptime\": %d, \"requests\": %d}\n", time_diff, server.requests);
    snprintf(r->status_code, sizeof(r->status_code), "%s", "200 OK");
    snprintf(r->type, sizeof(r->type), "%s", "application/json");

    return 0;
}

int handle_users(sqlite3 *sql_db, Clients *client, Response *r) {
    server.requests++;
 
    if (strcmp(client->method, "GET") == 0) {
        r->body[0] = '[';
        sqlite3_exec(sql_db, "SELECT * FROM users", callback_func, (void *)r, NULL);
        r->body[strlen(r->body)-2] = ']';
        GET_response(client, r);
  
    }
    else if (strcmp(client->method, "POST") == 0) {
        db_users(sql_db, client, r);
    }
    else if (strcmp(client->method, "DELETE") == 0) {
        DEL_users(sql_db, client, r);
    }
    else if (strcmp(client->method, "PUT") == 0) {
        UPDATE_users(sql_db, client, r);
    }

    return 0;
}

int db_users(sqlite3 *sql_db, Clients *client, Response *r) {
    uuid_t user_id;
    uuid_generate(user_id);
    char out[37];
    uuid_unparse(user_id, out);
    
    char buff_time[BUFF_SIZE];
    memset(buff_time, 0, sizeof(buff_time));
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buff_time, sizeof(buff_time), "%d-%m-%Y %H:%M:%S", t);

    char *username;
    char *password;
    char *p = strchr(client->body, ':') + 2;
    username = p;    
    p = strchr(p, '"');
    *p = '\0';  
    p = strchr(p+1, ':') + 2;  
    password = p;    
    p = strchr(p, '"');
    *p = '\0';  

    password = hashing_passwd(password, r);

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, BUFF_DB_SIZE, "INSERT INTO users (UUID, username, password, timestamp) VALUES ('%s', '%s', '%s', '%s')", 
            out, username, password, buff_time);
    sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);

    snprintf(r->status_code, sizeof(r->status_code), "%s", "201 Created");
    snprintf(r->type, sizeof(r->type), "%s", "application/json");  
    snprintf(r->body, sizeof(r->body), "{\"uuid\":\"%s\"}\n", out);

    return 0;
}

int callback_func(void *callback_data, int num_columns, char** values, char** col_names) {
    // printf("values: %s, %s, %s\n", values[0], values[1], values[2]);

    Response *r = (Response *)callback_data;
    snprintf(r->body + strlen(r->body), sizeof(r->body), 
             "{\"uuid\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\"},\n",
             values[0], values[1], values[3]);

    return 0;
}

int GET_response(Clients *client, Response *r) {    
    
    snprintf(r->status_code, sizeof(r->status_code), "%s", "200 OK");
    snprintf(r->type, sizeof(r->type), "%s", "application/json");  

    return 0;    
}

int DEL_users(sqlite3 *sql_db, Clients *client, Response *r) {   

    jwt_t *jwt;
    const char *jwt_key = getenv("JWT_SECRET");
    if (jwt_decode (&jwt, client->token, jwt_key, strlen(jwt_key)) != 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "401 Unauthorized");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"Unauthorized\"}\n");
        return 0;
    }   
    
    char *uuid = strrchr(client->path, '/') + 1;

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, sizeof(buff_db), "DELETE FROM users WHERE UUID='%s'", uuid);
    sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
    

    if (sqlite3_changes(sql_db) == 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "404 Not Found");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"User not found\"}\n");
    }
    else snprintf(r->status_code, sizeof(r->status_code), "%s", "204 No Content");

    return 0;
} 

int UPDATE_users(sqlite3 *sql_db, Clients *client, Response *r) {

    // printf("%s\n", client->buff);
    jwt_t *jwt;
    const char *jwt_key = getenv("JWT_SECRET");
    if (jwt_decode (&jwt, client->token, jwt_key, strlen(jwt_key)) != 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "401 Unauthorized");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"Unauthorized\"}\n");
        return 0;
    }   

    char *uuid = strrchr(client->path, '/') + 1;

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, sizeof(buff_db), "UPDATE users SET username = '%s' WHERE UUID= '%s'", client->body, uuid);     
    sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);

    if (sqlite3_changes(sql_db) == 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "404 Not Found");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"User not found\"}\n");
    }
    else snprintf(r->status_code, sizeof(r->status_code), "%s", "204 No Content");

    return 0;
}

char *hashing_passwd(char *passwd, Response *r) {

    static char out[crypto_pwhash_STRBYTES];
    unsigned long long passwdlen = strlen(passwd);

    if (crypto_pwhash_str(out, passwd, passwdlen, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "500 Internal Server Error");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"Internal server error\"}\n");      
    }

    return out;
}

int handle_login(sqlite3 *sql_db, Clients *client, Response *r) {
    server.requests++;

    char *username;
    char *password;
    char *p = strchr(client->body, ':') + 2;
    username = p;    
    p = strchr(p, '"');
    *p = '\0';  
    p = strchr(p+1, ':') + 2;  
    password = p;    
    p = strchr(p, '"');
    *p = '\0';  

    const char *hashed_password;
    const char *user_id;

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, BUFF_DB_SIZE, "SELECT password, UUID FROM users WHERE username = ('%s')", username);

    sqlite3_stmt *ppStmt;
    sqlite3_prepare_v2(sql_db, buff_db, BUFF_DB_SIZE, &ppStmt, NULL);
    if (sqlite3_step(ppStmt) != SQLITE_ROW) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "401 Unauthorized");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"Invalid credentials\"}\n");   
    }
    else {
        hashed_password = sqlite3_column_text(ppStmt, 0);
        user_id = sqlite3_column_text(ppStmt, 1);
    }    

    if (crypto_pwhash_str_verify (hashed_password, password, strlen(password)) != 0) {
        snprintf(r->status_code, sizeof(r->status_code), "%s", "401 Unauthorized");
        snprintf(r->type, sizeof(r->type), "%s", "application/json");
        snprintf(r->body, sizeof(r->body), "%s", "{\"error\": \"Invalid credentials\"}\n"); 
    }
    else {
        JWT_Token(user_id, r);
    }

    return 0;
}

int JWT_Token(const char *user_id, Response *r) {

    const char *jwt_key = getenv("JWT_SECRET");
    // printf("secret: %s\n", jwt_key);

    jwt_t *jwt;
    jwt_new(&jwt); 	
    
    jwt_set_alg(jwt, JWT_ALG_HS256, jwt_key, strlen(jwt_key));

    jwt_add_grant(jwt, "sub", user_id);
    jwt_add_grant_int(jwt, "exp", time(NULL)+3600);
    char *signed_token_str = jwt_encode_str(jwt);

    snprintf(r->status_code, sizeof(r->status_code), "%s", "200 OK");
    snprintf(r->type, sizeof(r->type), "%s", "application/json");
    snprintf(r->body, sizeof(r->body), "{\"token\": \"%s\"}\n", signed_token_str);
    
    jwt_free(jwt);
    return 0;
}




