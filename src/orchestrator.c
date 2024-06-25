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

Task tasks[MAX_TASKS];
int next_id = 0;
char output_folder[256];
int sched_policy;

int counter_max_parallel = 0;
int counter_tasks = 0;

#define MAX_COMMANDS 10
#define MAX_COMMAND_LENGTH 100

void handleError(char* error_msg) {
    write(STDERR_FILENO, error_msg, strlen(error_msg));
    exit(EXIT_FAILURE);
}

void deleteFilesInDirectory(const char *dir_path) {
    DIR *dp;
    struct dirent *entry;
    char path[1024];
    dp = opendir(dir_path);
    if (dp == NULL) {
        handleError("Error opening directory\n");
    }
    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int len = strlen(dir_path);
            strcpy(path, dir_path);
            if (path[len - 1] != '/') {
                path[len] = '/';
                len++;
            }
            strcpy(path + len, entry->d_name);
            if (remove(path) == -1) {
                handleError("Error deleting file\n");
            }
        }
    }
    closedir(dp);
}

void trim(char *str) {
    int len = strlen(str);
    if (len == 0)
        return;

    int start = 0;
    while (str[start] == ' ' || str[start] == '\t')
        start++;

    int end = len - 1;
    while (end >= 0 && (str[end] == ' ' || str[end] == '\t'))
        end--;

    if (start > 0) {
        for (int i = start; i <= end; i++)
            str[i - start] = str[i];
    }

    str[end - start + 1] = '\0';
}

char **parseCommands(const char *command, int *count) {
    char **commands = (char **)malloc(MAX_COMMANDS * sizeof(char *));
    if (commands == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    char *token;
    char *command_copy = strdup(command);

    token = strtok(command_copy, "|");

    int i = 0;
    while (token != NULL && i < MAX_COMMANDS) {
        commands[i] = (char *)malloc((strlen(token) + 1) * sizeof(char));
        if (commands[i] == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        strcpy(commands[i], token);
        trim(commands[i]);

        token = strtok(NULL, "|");
        i++;
    }

    *count = i;

    free(command_copy);

    return commands;
}

char **parseSingleCommand(const char *command) {
    #define MAX_ARGS 20

    char **args = (char **)malloc(MAX_ARGS * sizeof(char *));
    if (args == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    char *token = strtok(strdup(command), " ");
    int i = 0;

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i] = (char *)malloc((strlen(token) + 1) * sizeof(char));
        if (args[i] == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        strcpy(args[i], token);
        token = strtok(NULL, " ");
        i++;
    }
    args[i] = NULL;

    return args;
}

void addExecutionToQueue(Task task){
    task.id = next_id;
    task.status = 0;
    task.waiting_counter = 10;

    char id_str[20];
    int pid_length = snprintf(id_str, sizeof(id_str), "%d", task.id); 
    write(STDOUT_FILENO, "Process ", 8);
    write(STDOUT_FILENO, id_str, pid_length);
    write(STDOUT_FILENO, " add to the queue\n", 18);

    tasks[next_id] = task;

    char fifo_name[50];
    sprintf(fifo_name, "client_%d", task.pid);

    mkfifo(fifo_name, 0644);

    int fd_client = open(fifo_name, O_WRONLY);
    if(fd_client == -1){
        perror("Error opening FIFO for writing");
        exit(EXIT_FAILURE);
    }

    write(fd_client, &next_id, sizeof(int));

    next_id++;

    if (close(fd_client) == -1) {
        handleError("Error closing FIFO in addExecutionToQueue\n");
    }
}

void handleStatus(Task *tasks, Task task) {

    if (fork() == 0){
        char fifo_name[50];
        sprintf(fifo_name, "client_%d", task.pid);

        mkfifo(fifo_name, 0644);

        int fd_client = open(fifo_name, O_WRONLY);
        if(fd_client == -1){
            handleError("Error opening FIFO for writing in handleStatus\n");
        }

        write(fd_client, "\nScheduled:\n", 12);
        for (int i = 0; i <= next_id - 1; i++) {
            if (tasks[i].status == 0) {
                char output_msg[300];
                char* flag = (tasks[i].flag == 0) ? "-u" : (tasks[i].flag == 1) ? "-p" : "Invalid input";
                snprintf(output_msg, sizeof(output_msg), "Process %d, with the flag %s and command: %s\n", tasks[i].id, flag, tasks[i].command);
                write(fd_client, output_msg, strlen(output_msg));
            }
        }

        write(fd_client, "\nExecuting:\n", 12);
        for (int i = 0; i <= next_id; i++) {
            if (tasks[i].status == 1) {
                char output_msg[300];
                char* flag = (tasks[i].flag == 0) ? "-u" : (tasks[i].flag == 1) ? "-p" : "Invalid input";
                snprintf(output_msg, sizeof(output_msg), "Process %d, with the flag %s and command: %s\n", tasks[i].id, flag, tasks[i].command);
                write(fd_client, output_msg, strlen(output_msg));
            }
        }

        write(fd_client, "\nCompleted:\n", 12);
        for (int i = 0; i <= next_id; i++) {
            if (tasks[i].status == 2) {
                char output_msg[300];
                char* flag = (tasks[i].flag == 0) ? "-u" : (tasks[i].flag == 1) ? "-p" : "Invalid input";
                snprintf(output_msg, sizeof(output_msg), "Process %d, with the flag %s and command: %s, and with a run time of %ld ms\n", tasks[i].id, flag, tasks[i].command, tasks[i].runtime_ms);
                write(fd_client, output_msg, strlen(output_msg));
            }
        }

        if (close(fd_client) == -1) {
            handleError("Error closing FIFO in handleStatus\n");
        }
        unlink(fifo_name);

        _exit(1);
    }
}

int compare_tasks_0(const void *a, const void *b) {
    const Task *task1 = (const Task *)a;
    const Task *task2 = (const Task *)b;

    // Tasks with status 0 come first
    if (task1->status != task2->status) {
        return task1->status == 0 ? -1 : 1;
    }

    // If status is the same, tasks with waiting_counter <= 0 come first
    if (task1->waiting_counter <= 0 && task2->waiting_counter > 0) {
        return -1;
    } else if (task1->waiting_counter > 0 && task2->waiting_counter <= 0) {
        return 1;
    }

    // If waiting_counter is the same or both > 0, compare predict_time
    return task1->predict_time - task2->predict_time;
}

int compare_tasks_1(const void *a, const void *b) {
    const Task *task1 = (const Task *)a;
    const Task *task2 = (const Task *)b;

    // Tasks with status 0 come first
    if (task1->status != task2->status) {
        return task1->status == 0 ? -1 : 1;
    }

    // Tasks waiting longer come first (first come, first serve)
    return task1->waiting_counter - task2->waiting_counter;
}

void orderTasks(){
    if (sched_policy == 0) 
        qsort(tasks, next_id, sizeof(Task), compare_tasks_0);
    else if (sched_policy == 1)
        qsort(tasks, next_id, sizeof(Task), compare_tasks_1);
}

void handleExecute(Task parallel_task) {
    int i;
    int num_processes;

    char *command = parallel_task.command;
    char **commands = parseCommands(command, &num_processes);

    int num_pipes = num_processes - 1;
    int pipefds[num_pipes][2];

    for (i = 0; i < num_pipes; i++) {
        if (pipe(pipefds[i]) == -1) {
            handleError("Error creating pipe\n");
        }
    }

    for (i = 0; i < num_processes; i++)
    {
        pid_t pid = fork();

        if (pid == -1) {
            handleError("Error forking process\n");
        }
    
        if (pid == 0)
        {
            if (i > 0){
                if (dup2(pipefds[i-1][0], STDIN_FILENO) == -1) {
                    handleError("Error duplicating file descriptor for STDIN\n");
                }
            }

            if (i < num_processes - 1){
                if (dup2(pipefds[i][1], STDOUT_FILENO) == -1) {
                    handleError("Error duplicating file descriptor for STDOUT\n");
                }
            }

            else {
                char result_path[256];
                snprintf(result_path, sizeof(result_path), "../src/%s/outputFile_id_%d", output_folder, parallel_task.id);
                
                int result_fd = open(result_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (result_fd == -1) {
                    handleError("Error opening result file\n");
                }
                if (dup2(result_fd, STDOUT_FILENO) == -1) {
                    handleError("Error duplicating file descriptor for STDOUT\n");
                }
            }

            for (int j = 0; j < num_pipes; j++){
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            char **input = parseSingleCommand(commands[i]);
            if (input == NULL) handleError("Error parsing command\n");

            execvp(input[0], input);

            _exit(1);
        }
    }

    for (i = 0; i < num_pipes; i++){
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }
        

    for (i = 0; i < num_processes; i++)
        if (wait(NULL) == -1) {
            handleError("Error waiting for child process\n");
        }
}

int handleParallelTasks(int parallelTasksNum) {
    orderTasks(sched_policy);

    if (counter_max_parallel < parallelTasksNum && next_id > counter_tasks) {

        for (int i = 0; i < next_id; i++){
            if (tasks[i].status == 0) tasks[i].waiting_counter--;
        }

        counter_max_parallel++;
        counter_tasks++;
        tasks[0].status = 1;

        if (fork() == 0) {
            struct timeval start, end;
            gettimeofday(&start, NULL);

            handleExecute(tasks[0]);

            int server_fd = open(SERVER, O_WRONLY);
            if (server_fd == -1) {
                handleError("Error opening FIFO for writing\n");
            }

            tasks[0].flag = 3;

            gettimeofday(&end, NULL);
            long seconds = end.tv_sec - start.tv_sec;
            long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

            tasks[0].runtime_ms = micros;

            if (write(server_fd, &tasks[0], sizeof(Task)) == -1) {
                handleError("Error writing to FIFO\n");
                close(server_fd);
            }

            char info_file_path[256];
            snprintf(info_file_path, sizeof(info_file_path), "../src/%s/info", output_folder);
            int info_fd = open(info_file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (info_fd == -1) {
                handleError("Error opening info file\n");
            }

            char info_msg[256];
            snprintf(info_msg, sizeof(info_msg), "Task ID: %d, Runtime: %ld microseconds\n", tasks[0].id, micros);
            if (write(info_fd, info_msg, strlen(info_msg)) == -1) {
                handleError("Error writing to info file\n");
            }

            if (close(info_fd) == -1) {
                handleError("Error closing info file\n");
            }

            if (close(server_fd) == -1) {
                handleError("Error closing FIFO\n");
            }

            _exit(0);
        }
        return 1;
    }
    return 0;
}

int findTaskIndexById(int id) {
    for (int i = 0; i < next_id; i++) {
        if (tasks[i].id == id) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char * argv[]) {

    if (argc != 4) {
        handleError("Usage: <directory_name> <parallel_tasks> <scheduling_policy>\n");
    }

    strcpy(output_folder, argv[1]);

    struct stat st;
    char src_folder[256];
    snprintf(src_folder, sizeof(src_folder), "../src/%s", output_folder);

    if (stat(src_folder, &st) == 0 && S_ISDIR(st.st_mode)) {
        deleteFilesInDirectory(src_folder);
        char error_path[256];
        snprintf(error_path, sizeof(error_path), "%s/error", src_folder);
        FILE *error_file = fopen(error_path, "w");
        if (error_file == NULL) {
            handleError("Error opening error file\n");
        }
        fclose(error_file);
    } else {
        if (mkdir(src_folder, 0777) == -1) {
            handleError("Error creating directory\n");
        }
    }

    char error_path[256];
    snprintf(error_path, sizeof(error_path), "%s/error", src_folder);
    int error_fd = open(error_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (error_fd == -1) {
        handleError("Error opening error file\n");
    }
    dup2(error_fd, STDERR_FILENO);

    int parallel_tasks = atoi(argv[2]);
    if (parallel_tasks == 0) {
        handleError("Invalid parallel tasks number\n");
    }

    sched_policy = atoi(argv[3]);
    if (sched_policy != 0 && sched_policy != 1) {
        handleError("Invalid scheduling policy\n");
    }

    Task task;

    mkfifo(SERVER, 0666);

    int fifo_fd = open(SERVER, O_RDONLY);
    if(fifo_fd == -1){
        handleError("Error opening FIFO for reading\n");
    }

    int bytes_read;

    while (1)
    {
        if ((bytes_read = read(fifo_fd, &task, sizeof(Task))) > 0)
        {
            if (task.flag == 0 || task.flag == 1){
                addExecutionToQueue(task);
            } else if (task.flag == 2) {
                handleStatus(tasks, task);
            } else if (task.flag == 3) {
                int index = findTaskIndexById(task.id);
                tasks[index].status = 2;
                tasks[index].runtime_ms = task.runtime_ms;
                counter_max_parallel--;
                wait(NULL);
            } else {
                handleError("Error handling task\n");
            }
        }
        while (handleParallelTasks(parallel_tasks) == 1);
    }
}