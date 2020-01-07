//
// Created by pego149 on 4. 1. 2020.
//
#include <pthread.h>

typedef struct data {
    int sock;
    int kod;
    int portpasv;
    pthread_mutex_t *mutex;
} DATA;

void *recv_ftp(void *in);
void *ftp_enter_pasv(void *in);
int main();