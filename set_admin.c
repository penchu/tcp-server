#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sodium.h>
#include <time.h>
#include <uuid/uuid.h>

int main(void) {

    uuid_t user_id;
    uuid_generate(user_id);
    char out[37];
    uuid_unparse(user_id, out);
    
    char buff_time[128];
    memset(buff_time, 0, sizeof(buff_time));
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buff_time, sizeof(buff_time), "%d-%m-%Y %H:%M:%S", t);

    sqlite3 *sql_db;
    // sqlite3_open("monit_users.db", &sql_db);
    // sqlite3_exec(sql_db, "CREATE TABLE IF NOT EXISTS users (UUID TEXT PRIMARY KEY, username TEXT UNIQUE, password TEXT, timestamp TEXT, is_admin INTEGER)", 
    //         NULL, NULL, NULL); 

    char *username = "admin";
    char *password = "admin";
    int is_admin = 1;

    static char passwrd_hash[crypto_pwhash_STRBYTES];
    unsigned long long passwdlen = strlen(password);
    if (crypto_pwhash_str(passwrd_hash, password, passwdlen, crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        fprintf(stderr, "Hashing failed\n");
        return 1;
    }

    char buff_db[512];
    memset(buff_db, 0, sizeof(buff_db));
    // snprintf(buff_db, 512, "INSERT INTO users (UUID, username, password, timestamp, is_admin) VALUES ('%s', '%s', '%s', '%s', '%d')", 
    //         out, username, passwrd_hash, buff_time, is_admin);
    // if (sqlite3_exec(sql_db, buff_db, NULL, NULL, NULL) !=0) {
    //     fprintf(stderr, "%s\n", sqlite3_errmsg(sql_db));
    // }
    

    return 0;
}