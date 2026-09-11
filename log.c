#include <stdio.h>
#include "log.h"

static FILE *log_fp = NULL;

int log_init();
int log_close();

int log_init() {
    
    log_fp = fopen("logs.log", "a");
    return 0;
}

int log_close() {
    fclose(log_fp);
    log_fp = NULL;
    return 0;
}