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
#include <errno.h>

#define MAX_CLIENTS 10
#define BUFF_SIZE 128
#define BUFF_DB_SIZE 512
#define MIN_SIZE 16
#define MAX_PASSWORD_LENGTH 256


typedef struct {
    int cl_fd;
    char *buff;
    int position;
    char method_arr[MIN_SIZE];
    char path_arr[BUFF_SIZE*2];
    char version_arr[MIN_SIZE];
    char token_arr[BUFF_SIZE*4];
    char username[BUFF_SIZE/2];
    char password[BUFF_SIZE*2];
    int send_position;
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
    char *body;
    char type[BUFF_SIZE];
    int pos_body;
    int capacity;
    int error;
} Response;

Server server;

int server_init(int *sockfd);
int server_run(int *sockfd);
int handle_new_client(int *sockfd, int *max_fd, fd_set *master_set);
int handle_client_data(Clients *client, char *buff, int rcv_srvr);
int http_header_parse(Clients *client);
int handle_request(Clients *client, char *pass);
int handle_health(Clients *client, Response *r);
int handle_metrics(Clients *client, Response *r);
int handle_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass);
int db_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass);
int callback_func(void *callback_data, int num_columns, char** values, char** col_names);
int DEL_users(sqlite3 *sql_db, Clients *client, Response *r);
int UPDATE_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass);
char *hashing_passwd(Clients *client, char *pass);
int handle_login(sqlite3 *sql_db, Clients *client, Response *r, char *pass);
int JWT_Token(Clients *client, const char *user_id, int is_admin, Response *r);
int write_response(Clients *client, char *status_code, char *body, int len);
int log_event();

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
    
    if (listen(*sockfd, 15) < 0) {
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
    memset(buff, 0, sizeof(buff));
    int rcv_srvr;    

    Clients client_list[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_list[i].buff = NULL;
        client_list[i].position = 0;
    }
    
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
                    rcv_srvr = recv(i, buff, sizeof(buff)-1, 0);
                    if (rcv_srvr < 0) {
                        perror("receive");
                        continue;
                    }
                    else if (rcv_srvr > 0) {
                        client_list[i].cl_fd = i;
                        handle_client_data(&client_list[i], buff, rcv_srvr);
                    }
                    else {
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

int handle_client_data(Clients *client, char *buff, int rcv_srvr) {

    if (client->buff == NULL) {
        client->buff = malloc(rcv_srvr+1); // an error check should be added 
    }
    else {
        char *buff_temp = realloc(client->buff, client->position+rcv_srvr+1);
        if (buff_temp == NULL) {  
            int len = strlen("Internal server error"); 
            write_response(client, "500 Internal Server Error", "Internal server error", len);
            return 0;
        }
        else {
            client->buff = buff_temp;
        }
    }    

    memcpy(&client->buff[client->position], buff, rcv_srvr);
    client->position += rcv_srvr;
    client->buff[client->position] = '\0';
    
    char *content_len;
    char *zero_pos;
    int len = 0;
    int z_pos = 0;
    
    if ((zero_pos = strstr(client->buff, "\r\n\r\n"))) {   
        zero_pos += strlen("\r\n\r\n");        
        z_pos = zero_pos - client->buff;

        if ((content_len = strstr(client->buff, "Content-Length:"))){            
            content_len += strlen("Content-Length:");
            len = atoi(content_len);     
        }   
    
        if ((z_pos + len) == client->position) http_header_parse(client);            
    }

    return 0;
}

int http_header_parse(Clients *client) {

    char *position;
    int counter = 0;
    while (client->buff[counter] != ' ') {
        counter++;
    }   
    strncpy(client->method_arr, client->buff, counter);
    client->method_arr[counter] = '\0';

    position = client->buff + counter + 1;
    counter = 0;
    while (*(position + counter) != ' ') {
        counter++;
    }
    strncpy(client->path_arr, position, counter);
    client->path_arr[counter] = '\0';

    position += counter + 1;
    counter = 0;
    while (*(position + counter) != '\r') {
        counter++;
    }
    strncpy(client->version_arr, position, counter);
    client->version_arr[counter] = '\0';

    counter = 0;
    if ((position = strstr(client->buff, "Authorization: Bearer "))) {
        position += strlen("Authorization: Bearer ");
        while (*(position + counter) != '\r') {
            counter++;
        }
        strncpy(client->token_arr, position, counter);
        client->token_arr[counter] = '\0';
        counter = 0; 
    }   
    
    position = strstr(client->buff, "\r\n\r\n") + strlen("\r\n\r\n");
    if ((position = strstr(client->buff, "username"))) {
        position += strlen("username") + 3;
        while (*(position + counter) != '\"') {
            counter++;
        }
        strncpy(client->username, position, counter);
        client->username[counter] = '\0';
        counter = 0;
    }
    
    char temp_pass[MAX_PASSWORD_LENGTH];
    if ((position = strstr(client->buff, "password"))) {
        position += strlen("password") + 3;
        while (*(position + counter) != '\"') {
            counter++;
        }
        if (counter < MAX_PASSWORD_LENGTH) {
            strncpy(temp_pass, position, counter);
            temp_pass[counter] = '\0';
        }  
        else {
            int len = strlen("{\"error\":\"Password exceeds maximum length.\"}");
            write_response(client, "400 Bad Request", "{\"error\":\"Password exceeds maximum length.\"}", len);  
        }
    }
     
    free(client->buff);
    handle_request(client, temp_pass);
    
    return 0;
}

int handle_request(Clients *client, char *pass) {
    // printf("method: %s, path: %s, version: %s, body: %s\n", client->method, client->path, client->version, client->body);
    
    Response response;
    memset(&response, 0, sizeof(Response));
    
    sqlite3 *sql_db;
    sqlite3_open("monit_users.db", &sql_db);
    sqlite3_exec(sql_db, "CREATE TABLE IF NOT EXISTS users (UUID TEXT PRIMARY KEY, username TEXT UNIQUE, password TEXT, timestamp TEXT, is_admin INTEGER)", 
                NULL, NULL, NULL);  
    
    if (strcmp("/health", client->path_arr) == 0) {
        handle_health(client, &response);
    }
    else if (strcmp("/metrics", client->path_arr) == 0) {
        handle_metrics(client, &response);
    }
    else if (strstr(client->path_arr, "/users") != NULL) {
        handle_users(sql_db, client, &response, pass);
    }
    else if (strstr(client->path_arr, "/login") != NULL) {
        handle_login(sql_db, client, &response, pass);
    }

    sqlite3_close(sql_db);
    free(response.body);

    return 0;
}

int handle_health(Clients *client, Response *r) {
    server.requests++;
    int len = strlen("{\"status\": \"ok\"}");
    write_response(client, "200 OK", "{\"status\": \"ok\"}", len);

    return 0;
}

int handle_metrics(Clients *client, Response *r) {
    server.requests++;
    time_t now_2 = time(NULL);
    int time_diff = difftime(now_2, server.now);

    char uptime[BUFF_SIZE];
    int len = snprintf(uptime, BUFF_SIZE, "{\"uptime\": %d, \"requests\": %d}", time_diff, server.requests);
    write_response(client, "200 OK", uptime, len);

    return 0;
}

int handle_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass) {
    server.requests++;
    
    if (strcmp(client->method_arr, "GET") == 0) {
        r->body = calloc(BUFF_DB_SIZE+1, 1);

        r->capacity += BUFF_DB_SIZE;
        r->body[0] = '[';
        r->pos_body++;

        char *uuid;
        if ((uuid = strstr(client->path_arr, "users/"))) {  
            uuid += strlen("users/");

            char buff_db[BUFF_DB_SIZE];
            memset(buff_db, 0, sizeof(buff_db));
            snprintf(buff_db, BUFF_DB_SIZE, "SELECT username, timestamp FROM users WHERE UUID = ('%s')", uuid);

            sqlite3_stmt *ppStmt;
            sqlite3_prepare_v2(sql_db, buff_db, BUFF_DB_SIZE, &ppStmt, NULL);  

            if (sqlite3_step(ppStmt) != SQLITE_ROW) { 
                int len = strlen("{\"error\": \"User not found\"}");
                write_response(client, "404 Not Found", "{\"error\": \"User not found\"}", len);
                sqlite3_finalize(ppStmt);
                return 0;
            }
            else {
                const char *username = sqlite3_column_text(ppStmt, 0);
                const char *timestamp = sqlite3_column_text(ppStmt, 1);
                snprintf(r->body, r->capacity, "{\"uuid\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\"},\n",
                        uuid, username, timestamp);     
            }
            sqlite3_finalize(ppStmt);
        }
        else sqlite3_exec(sql_db, "SELECT * FROM users", callback_func, (void *)r, NULL);
        
        
        if (r->body[1] != '\0') {
            r->pos_body = r->pos_body - 2;
            r->body[r->pos_body] = ']';
            // r->body[r->pos_body-1] = '\n';            
        }
        else {
            r->body[1] = ']';
            r->pos_body++;
            // r->body[2] = '\n';
        }

        // for (int i = 0; r->body[i] != '\0'; i++) {
        //     printf("%02x ", r->body[i]);
        // }
        // printf("\n");
        // exit(0);

        if (r->error == 0) write_response(client, "200 OK", r->body, r->pos_body);
        else write_response(client, "500 Internal Server Error", "Internal server error", strlen("Internal server error"));

    }
    else if (strcmp(client->method_arr, "POST") == 0) {
        db_users(sql_db, client, r, pass);
    }
    else if (strcmp(client->method_arr, "DELETE") == 0) {
        DEL_users(sql_db, client, r);
    }
    else if (strcmp(client->method_arr, "PUT") == 0) {
        UPDATE_users(sql_db, client, r, pass);
    }

    return 0;
}

int db_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass) {
    uuid_t user_id;
    uuid_generate(user_id);
    char out[37];
    uuid_unparse(user_id, out);
    
    char buff_time[BUFF_SIZE];
    memset(buff_time, 0, sizeof(buff_time));
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buff_time, sizeof(buff_time), "%d-%m-%Y %H:%M:%S", t);

    char *password = hashing_passwd(client, pass);

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, BUFF_DB_SIZE, "INSERT INTO users (UUID, username, password, timestamp, is_admin) VALUES ('%s', '%s', '%s', '%s', '%s')", 
            out, client->username, password, buff_time, "0");

    sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);

    char db_body[BUFF_SIZE];
    memset(db_body, 0, BUFF_SIZE);
    int len = snprintf(db_body, BUFF_SIZE, "{\"uuid\":\"%s\"}", out);

    write_response(client, "201 Created", db_body, len);

    return 0;
}

int callback_func(void *callback_data, int num_columns, char** values, char** col_names) {
    // printf("values: %s, %s, %s\n", values[0], values[1], values[2]);
    
    Response *r = (Response *)callback_data;
    r->error = 0;

    int mem_body = snprintf(NULL, 0, "{\"uuid\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\"},\n", values[0], values[1], values[3]);

    if (mem_body + r->pos_body > r->capacity) {
        char *buff = realloc(r->body, r->capacity + mem_body + 1);
        if (buff == NULL) {   
            r->error = 1;
            return 0;
        }
        else {
            r->body = buff;
            r->capacity += mem_body;
            // r->body[r->capacity] = '\0'; // not sure about that
        }
    }

    r->pos_body += snprintf(r->body + r->pos_body, r->capacity - r->pos_body, 
             "{\"uuid\":\"%s\",\"username\":\"%s\",\"timestamp\":\"%s\"},\n", 
             values[0], values[1], values[3]); 

    return 0;
}

int DEL_users(sqlite3 *sql_db, Clients *client, Response *r) {   

    char *uuid = strrchr(client->path_arr, '/') + 1;
    jwt_t *jwt = NULL;
    const char *jwt_key = getenv("JWT_SECRET");

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));

    if (jwt_decode(&jwt, client->token_arr, jwt_key, strlen(jwt_key)) != 0) {
        int len = strlen("{\"error\": \"Unauthorized\"}");
        write_response(client, "401 Unauthorized", "{\"error\": \"Unauthorized\"}", len);
        jwt_free(jwt);
        return 0;
    }   

    if (jwt_get_grant_int(jwt, "adm") != 0) { 
        if (strcmp(jwt_get_grant(jwt, "sub"), uuid) != 0) {
            int len = strlen("{\"error\": \"Unauthorized\"}");
            write_response(client, "401 Unauthorized", "{\"error\": \"Unauthorized\"}", len);
            jwt_free(jwt);
            return 0; 
        }
        else {
            snprintf(buff_db, sizeof(buff_db), "DELETE FROM users WHERE UUID='%s'", uuid);
            sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
        }
    }
    else {
        snprintf(buff_db, sizeof(buff_db), "DELETE FROM users WHERE UUID='%s'", uuid);
        sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL); // probably a check should be added if it is successful 
    }
    

    if (sqlite3_changes(sql_db) == 0) {
        int len = strlen("{\"error\": \"User not found\"}");
        write_response(client, "404 Not Found", "{\"error\": \"User not found\"}", len);
    }
    else write_response(client, "204 No Content", NULL, 0);
    

    jwt_free(jwt);
    return 0;
} 

int UPDATE_users(sqlite3 *sql_db, Clients *client, Response *r, char *pass) {

    char *uuid = strrchr(client->path_arr, '/') + 1;
    jwt_t *jwt;
    const char *jwt_key = getenv("JWT_SECRET");

    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));

    if (jwt_decode(&jwt, client->token_arr, jwt_key, strlen(jwt_key)) != 0) {
        int len = strlen("{\"error\": \"Unauthorized2\"}");
        write_response(client, "401 Unauthorized", "{\"error\": \"Unauthorized1\"}", len);
        jwt_free(jwt);
        return 0;
    }    

    if (jwt_get_grant_int(jwt, "adm") == 0) { 
        if (strcmp(jwt_get_grant(jwt, "sub"), uuid) != 0) {
            int len = strlen("{\"error\": \"Unauthorized2\"}");
            write_response(client, "401 Unauthorized", "{\"error\": \"Unauthorized2\"}", len);
            jwt_free(jwt);
            return 0; 
        }
        else {
            if (client->username[0] != '\0') {
                snprintf(buff_db, sizeof(buff_db), "UPDATE users SET username = '%s' WHERE UUID= '%s'", client->username, uuid);     
                sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
            }    
            if (client->password[0] != '\0') {
                memset(buff_db, 0, sizeof(buff_db));
                char *password = hashing_passwd(client, pass);
                snprintf(buff_db, sizeof(buff_db), "UPDATE users SET password = '%s' WHERE UUID= '%s'", password, uuid);     
                sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
            }
        }
    }
    else {
        if (client->username[0] != '\0') {
            snprintf(buff_db, sizeof(buff_db), "UPDATE users SET username = '%s' WHERE UUID= '%s'", client->username, uuid);     
            sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
        }    
        if (client->password[0] != '\0') {
            memset(buff_db, 0, sizeof(buff_db));
            char *password = hashing_passwd(client, pass);
            snprintf(buff_db, sizeof(buff_db), "UPDATE users SET password = '%s' WHERE UUID= '%s'", password, uuid);     
            sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL);
        }
    }
   
    if (sqlite3_changes(sql_db) == 0) {
        int len = strlen("{\"error\": \"User not found\"}");
        write_response(client, "404 Not Found", "{\"error\": \"User not found\"}", len);
    }
    else write_response(client, "204 No Content", NULL, 0);
    jwt_free(jwt);
    return 0;
}

char *hashing_passwd(Clients *client, char *pass) {

    static char out[crypto_pwhash_STRBYTES];
    unsigned long long passlen = strlen(pass);

    if (crypto_pwhash_str(out, pass, passlen, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        int len = strlen("{\"error\": \"Internal server error\"}");
        write_response(client, "500 Internal Server Error", "{\"error\": \"Internal server error\"}", len);   
    }

    return out;
}

int handle_login(sqlite3 *sql_db, Clients *client, Response *r, char *pass) {
    server.requests++;

    const char *hashed_password;
    const char *user_id; 
    int is_admin;
    char buff_db[BUFF_DB_SIZE];
    memset(buff_db, 0, sizeof(buff_db));
    snprintf(buff_db, BUFF_DB_SIZE, "SELECT password, UUID, is_admin FROM users WHERE username = ('%s')", client->username);
    
    sqlite3_stmt *ppStmt;
    sqlite3_prepare_v2(sql_db, buff_db, BUFF_DB_SIZE, &ppStmt, NULL);    
    if (sqlite3_step(ppStmt) != SQLITE_ROW) {
        int len = strlen("{\"error\": \"Invalid credentials\"}");
        write_response(client, "401 Unauthorized", "{\"error\": \"Invalid credentials\"}", len);   
        sqlite3_finalize(ppStmt);
        return 0;
    }
    else {
        hashed_password = sqlite3_column_text(ppStmt, 0);
        user_id = sqlite3_column_text(ppStmt, 1);
        is_admin = sqlite3_column_int(ppStmt, 2);
    }   

    if (crypto_pwhash_str_verify (hashed_password, pass, strlen(pass)) != 0) {
        int len = strlen("{\"error\": \"Invalid credentials\"}");
        write_response(client, "401 Unauthorized", "{\"error\": \"Invalid credentials\"}", len);
    }
    else {
        JWT_Token(client, user_id, is_admin, r);
    }
    
    sqlite3_finalize(ppStmt);
    return 0;
}

int JWT_Token(Clients *client, const char *user_id, int is_admin, Response *r) {

    const char *jwt_key = getenv("JWT_SECRET");

    jwt_t *jwt;
    jwt_new(&jwt); 	
    
    jwt_set_alg(jwt, JWT_ALG_HS256, jwt_key, strlen(jwt_key));

    jwt_add_grant(jwt, "sub", user_id);
    jwt_add_grant_int(jwt, "exp", time(NULL)+3600);
    jwt_add_grant_int(jwt, "adm", is_admin); //adding the value here
    char *signed_token_str = jwt_encode_str(jwt);

    char jwt_body[BUFF_SIZE*5];
    memset(jwt_body, 0, BUFF_SIZE*5);
    int len = snprintf(jwt_body, BUFF_SIZE*5, "{\"token\": \"%s\"}", signed_token_str);
    
    write_response(client, "200 OK", jwt_body, len);
    
    jwt_free(jwt);
    return 0;
}

int write_response(Clients *client, char *status_code, char *body, int len) {

    int pos = 0;
    int mem_alloc = 1;
    char *buff_send;    

    if (body != NULL) {

        mem_alloc += snprintf(NULL, 0, "HTTP/1.1 %s\r\n", status_code);
        mem_alloc += snprintf(NULL, 0, "Content-Length: %d\r\n\r\n", len);
        mem_alloc += len + strlen("Content-Type: application/json\r\n");

        buff_send = malloc(mem_alloc);

        pos += snprintf(buff_send, mem_alloc, "HTTP/1.1 %s\r\n", status_code);
        pos += snprintf(buff_send + pos, mem_alloc - pos, "Content-Type: application/json\r\n");
        pos += snprintf(buff_send + pos, mem_alloc - pos, "Content-Length: %d\r\n\r\n", len);
        pos += snprintf(buff_send + pos, mem_alloc - pos, "%s", body);
    }
    else {
        mem_alloc += snprintf(NULL, 0, "HTTP/1.1 %s\r\n\r\n", status_code);
        buff_send = malloc(mem_alloc);
        pos += snprintf(buff_send, mem_alloc, "HTTP/1.1 %s\r\n\r\n", status_code);
    }
    
    send(client->cl_fd, buff_send, pos, 0);
    free(buff_send);

    return 0;
}

int log_event() {

    return 0;
}


