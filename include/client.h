#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define SERVER "orchestrator_fifo"

typedef struct {
    int id;
    pid_t pid;
    char command[300];
    int flag; // 0: unica execução, 1: pipeline, 2: status
    int status; // 0: em espera, 1: em execução, 2: terminada
    int predict_time;
    int runtime_ms;
} Task;

int is_number(const char *str);

void handleExecute(char *argv[], int flag);

void handleStatus();

#endif /* CLIENT_H */