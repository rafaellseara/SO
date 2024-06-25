# Sistemas Operativos
---

## Grade: 17/20 ⭐
---

This Operating Systems project implements a task orchestration service, coordinating the execution of tasks submitted by a user to a server in a client-server model.

**Learning Outcomes**

- Client-Server Model: Implementing communication between client and server using FIFOs
- Task Submission: Sending individual tasks or pipelines via command line
- Task Scheduling: Implementing scheduling policies (FCFS and FFCFS)
- Parallel Execution: Managing parallel tasks using forks and pipes
- Status Query: Implementing status queries for tasks on the server
- Result Management: Redirecting task results to files
- Testing and Comparison: Evaluating the efficiency of scheduling policies with automated tests

---

To run the project, you need to run the make command and then go to the bin directory and pass the following commands:

* **./orchestrator results 3 1** -> The first argument is the folder created in the source for the results, the second argument is the number of tasks to run in parallel and the third argument is the type of scheduling (1 - First Come First Serve and 0 - Fast and First Come First Serve).
* **./client status** -> to check the status of the program
* **./client execute 50 -u “ls -l”** -> The second argument is the expected time, the third is the flag which can also be "-p", and the last is the command to be executed

To run the tests, you need to enter the bin directory and then execute the following commands:

* **chmod +x test.sh**
* **time ./test.sh**

---

## Developed by:

[Rafael Seara](https://github.com/rafaellseara)<br>
[Lara Regina](https://github.com/larareginaa)<br>
[Martim Melo](https://github.com/MartimMelo)<br>
