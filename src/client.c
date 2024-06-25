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
    int flag; // 0: single execution, 1: pipeline, 2: status
    int status; // 0: waiting, 1: executing, 2: completed
    int predict_time;
    long runtime_ms;
    int waiting_counter;
} Task;

int is_number(const char *str) {
    while (*str) {
        if (!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

void handleExecute(char *argv[], int flag){
    int fd;
    Task task;
    pid_t pid = getpid();

    task.pid = pid;
    task.predict_time = atoi(argv[2]);
    task.flag = flag;
    strcpy(task.command, argv[4]);

    fd = open(SERVER, O_WRONLY);
    if(fd == -1){
        perror("Error opening FIFO for writing");
        exit(EXIT_FAILURE);
    }

    write(fd, &task, sizeof(Task));

    close(fd);

    char fifo_name[50];
    sprintf(fifo_name, "client_%d", pid);

    mkfifo(fifo_name, 0644);
    int fd_client;

    fd_client = open(fifo_name, O_RDONLY);
    if(fd_client == -1){
        perror("Error opening FIFO for reading");
        exit(EXIT_FAILURE);
    }

    int id;
    read(fd_client, &id, sizeof(int));


    char id_str[20];
    snprintf(id_str, sizeof(id_str), "%d", id);

    write(STDOUT_FILENO, "Your Task was submitted sucessfuly.\nProcess ID: ", 48);
    write(STDOUT_FILENO, id_str, strlen(id_str));
    write(STDOUT_FILENO, "\n", 1);

    close(fd_client);
    unlink(fifo_name);
}

void handleStatus(){  

    int fd;
    Task task;
    pid_t pid = getpid();

    task.pid = pid;
    task.flag = 2;

    fd = open(SERVER, O_WRONLY);
    if(fd == -1){
        perror("Error opening FIFO for writing");
        exit(EXIT_FAILURE);
    }

    write(fd, &task, sizeof(Task));

    close(fd);

    char fifo_name[50];
    sprintf(fifo_name, "client_%d", pid);

    mkfifo(fifo_name, 0644);
    int fd_client;
    
    fd_client = open(fifo_name, O_RDONLY);
    if(fd_client == -1){
        perror("Error opening FIFO for reading");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(fd_client, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    close(fd_client);
    unlink(fifo_name);
}

int main(int argc, char * argv[]){

    if (argc == 2 && (strcmp(argv[1], "status") == 0)) {
        handleStatus();
	} else if (argc == 5 && (strcmp(argv[1], "execute") == 0) && is_number(argv[2])
                && ((strcmp(argv[3], "-u") == 0) || (strcmp(argv[3], "-p") == 0))){
        
        if (strcmp(argv[3], "-u") == 0) handleExecute(argv, 0);
        else handleExecute(argv, 1);
    } else {
        printf("Invalid command.\n");
		_exit(1);
    }

    return 0;
}