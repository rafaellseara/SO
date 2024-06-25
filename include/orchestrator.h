#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/time.h>

#define SERVER "orchestrator_fifo"
#define MAX_TASKS 200

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

extern Task tasks[MAX_TASKS];
extern int next_id;
extern char output_folder[256];

extern int counter_max_parallel;
extern int counter_tasks;

void handleError(char* error_msg);
void deleteFilesInDirectory(const char *dir_path);
void trim(char *str);
char **parseCommands(const char *command, int *count);
char **parseSingleCommand(const char *command);
void addExecutionToQueue(Task task);
void handleStatus(Task *tasks, Task task);
int compare_tasks(const void *a, const void *b);
void orderTasks();
void handleExecute(Task parallel_task);
int handleParallelTasks(int parallelTasksNum);
int findTaskIndexById(int id);

#endif /* ORCHESTRATOR_H */