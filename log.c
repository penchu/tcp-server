#include <stdio.h>
#include <string.h>
#include <time.h>
#include "log.h"

#define BUFF_SIZE 128

static FILE *log_fp = NULL;

void log_init(void);
void log_info(char *body);
void log_error(char *body);
void timestamp(char *buff);
void log_close(void);

void log_init(void) {
    log_fp = fopen("logs.log", "a");
}

void log_info(char *body) {    
    char buff_time[BUFF_SIZE];
    memset(buff_time, 0, sizeof(buff_time));
    timestamp(buff_time);
    fprintf(log_fp, "%s [INFO] %s\n", buff_time, body);
    fflush(log_fp);
}

void log_error(char *body) {
    char buff_time[BUFF_SIZE];
    memset(buff_time, 0, sizeof(buff_time));
    timestamp(buff_time);
    fprintf(log_fp, "%s [ERROR] %s\n", buff_time, body);
    fflush(log_fp);
}

void timestamp(char *buff) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buff, BUFF_SIZE, "%d-%m-%Y %H:%M:%S", t);
}

void log_close() {
    fclose(log_fp);
    log_fp = NULL;
}