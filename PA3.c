#define _CRT_SECURE_NO_WARNINGS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Struct of a process*/
typedef struct process 
{
    uint8_t pid; // unit8_t and unit64_t are used to avoid data loss when the environment changes
    uint8_t priority;
    uint64_t arrivalTime;
    uint64_t burstTime;
    uint64_t initialRunningTime;
    uint64_t notRunningSince;
    uint64_t remainingBurstTime;
} process;

/*Struct of a process node for the queue*/
typedef struct processNode 
{
    process* process;
    struct processNode* prev;
    struct processNode* next;
} processNode;

/*Struct of the process node's queue*/
typedef struct processQueue 
{
    processNode* head;
    processNode* tail;
    size_t size;
} processQueue;

/*Struct of context of scheduling*/
typedef struct context 
{
    processQueue jobQueue;
    processQueue readyQueue;
    process* currentProcess;
    uint64_t elapsedTime;
    uint64_t currentProcessStartTime;
} context;

/*Struct of scheduler to save context states*/
typedef struct scheduler 
{
    process* (*whatToStart)(const context* ctx, void* data);
    int (*whetherToStop)(const context* ctx, void* data);
    void* data;
} scheduler;

/*Struct of dispathcer for context switching*/
typedef struct dispatcher
{
    context context;
    const scheduler* scheduler;
} dispatcher;

/*Struct of calculating output data*/
typedef struct stats 
{
    double averageCpuUsage;
    double averageWaitingTime;
    double averageResponseTime;
    double averageTurnaroundTime;
} stats;

/*Funtion push to push node to queue*/
void push(processQueue* queue, process* process)
{
    processNode* node = malloc(sizeof(processNode)); // create new node and input node
    node->process = process;
    node->prev = queue->tail;
    node->next = NULL;
    if (queue->size > 0) // if queue is  empty
    {
        queue->tail->next = node;
        queue->tail = node;
    }
    else 
    {
        queue->head = node;
        queue->tail = node;
    }
    queue->size += 1;
}

/*Funtion pop to bring the node from the queue*/
process* pop(processQueue* queue, processNode* node)
{
    if (queue->head == node) // if the node exists in frist queue
    {
        queue->head = node->next;
    }
    else
    {
        node->prev->next = node->next;
    }

    if (queue->tail == node) // if the node exist in last queue
    {
        queue->tail = node->prev;
    }
    else
    {
        node->next->prev = node->prev;
    }

    process* process = node->process;
    free(node);
    queue->size -= 1;
    return process; // return poped node
}

/*Funtion find to whrere the node exist*/
processNode* find(processQueue* queue, process* process) 
{
    for (processNode* i = queue->head; i != NULL; i = i->next)
    {
        if (i->process == process)  // if find the node
            return i; // return the node
    }
    return NULL;
}

/*Funtion clear to reuse the queue, free all nodes in the queue and initialize the queue */
void clear(processQueue* queue) 
{
    processNode* node = queue->head;
    while (node != NULL) 
    {
        processNode* next_node = node->next;
        free(node);
        node = next_node;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

/*Funtion makeContext to create and set new context*/
context makeContext(size_t jobCount, const process jobs[])
{
    processQueue jobQueue = { NULL, NULL, 0 };
    processQueue readyQueue = { NULL, NULL, 0 };
    for (size_t i = 0; i < jobCount; ++i) // Filling the jobs in the array jobs into the job queue
    {
        process* job = malloc(sizeof(process));
        memcpy(job, &jobs[i], sizeof(process)); // copy the memory of process to array of jobs
        push(&jobQueue, job);
    }
    context context = { jobQueue, readyQueue, NULL, 0 };
    return context;
}

/* Funtion makeDispatcher to set the dispatcher*/
dispatcher makeDispatcher(context context, const scheduler* scheduler)
{
    const dispatcher dispatcher = { context, scheduler };
    return dispatcher;
}

/* Funtion printCurrentTime to print current time to output FILE*/
void printCurrentTime(const context* ctx, FILE* output_file) 
{
    fprintf(output_file, "<time %" PRIu64 ">\t", ctx->elapsedTime);
}

/* Funtion step to run process and print some data in single time*/
int step(dispatcher* dispatcher, processQueue* finishedProcesses, FILE* output_file)
{
    context* ctx = &dispatcher->context;
    const scheduler* scheduler = dispatcher->scheduler;

    if (ctx->currentProcess != NULL && ctx->elapsedTime - ctx->currentProcessStartTime >= ctx->currentProcess->remainingBurstTime) //if the current process is finished
    {
        process* process = pop(&ctx->readyQueue, find(&ctx->readyQueue, ctx->currentProcess)); // take the process from ready queue

        process->notRunningSince = ctx->elapsedTime;
        process->remainingBurstTime = process->remainingBurstTime - ctx->elapsedTime + ctx->currentProcessStartTime;
        push(finishedProcesses, process); // push finishedProcesses queue to calculate output data

        printCurrentTime(ctx, output_file);
        fprintf(output_file, "[FINISHED] Process %" PRIu8 "\n", process->pid);
        fprintf(output_file, "-------------------------------------------\n");
        ctx->currentProcess = NULL;
    }

    if (ctx->jobQueue.size == 0 && ctx->readyQueue.size == 0) // if all processes are finished
    {
        printCurrentTime(ctx, output_file);
        fprintf(output_file, "[All FINISHED]\n");
        return 0; // return 0 to finish funtion run's while loop
    }

    if (ctx->currentProcess != NULL && scheduler->whetherToStop(ctx, scheduler->data)) // Stop process if the scheduler system call, save the data in context variable ctx
    {
        ctx->currentProcess->notRunningSince = ctx->elapsedTime;
        ctx->currentProcess->remainingBurstTime -= ctx->elapsedTime - ctx->currentProcessStartTime;
        printCurrentTime(ctx, output_file);
        fprintf(output_file, "[STOP] Process %" PRIu8 "\n", ctx->currentProcess->pid);
        fprintf(output_file, "-------------------------------------------\n");
        ctx->currentProcess = NULL;
    }

    processNode* node = ctx->jobQueue.head; // Push arrived processes into ready queue.
    while (node != NULL) 
    {
        if (node->process->arrivalTime <= ctx->elapsedTime) //if new processes arrived the time, pop the processes from job queue to push ready queue
        {
            processNode* next_node = node->next;
            process* process = pop(&ctx->jobQueue, node);
            process->notRunningSince = ctx->elapsedTime;
            push(&ctx->readyQueue, process);
            printCurrentTime(ctx, output_file);
            fprintf(output_file, "[READY] Process %" PRIu8 "\n", process->pid);
            node = next_node;
        }
        else
        {
            node = node->next;
        }
    }

    if (ctx->currentProcess == NULL && ctx->readyQueue.size > 0) // Start process the scheduler wants if no process is running.
    {
        ctx->currentProcess = scheduler->whatToStart(ctx, scheduler->data);
        ctx->currentProcessStartTime = ctx->elapsedTime;

        printCurrentTime(ctx, output_file);
        fprintf(output_file, "[START] Process %" PRIu8 "\n", ctx->currentProcess->pid);
    }

    printCurrentTime(ctx, output_file); // Print current process

    if (ctx->currentProcess == NULL)
    {
        fprintf(output_file, "[Idle]\n");
    }
    else 
    {
        fprintf(output_file, "[RUNNING] Process %" PRIu8 "\n", ctx->currentProcess->pid);
    }

    ctx->elapsedTime += 1; //count elapsed time
    return 1;
}

/* Funtion run to run process and return finished process throgh step funtion*/
processQueue run(dispatcher* dispatcher, FILE* output_file)
{
    processQueue finishedProcesses = { NULL, NULL, 0 };
    while (step(dispatcher, &finishedProcesses, output_file)) // if all processes are finished
    {}
    return finishedProcesses; //return finished processess queue. They have data.
}

// Funtion to find first process in scheduling FCFS
process* fcfsWhatToStart(const context* ctx, void* data)
{
    return ctx->readyQueue.head->process;
}

// Funtion to set stop in scheduling FCFS
int fcfsWhetherToStop(const context* ctx, void* data)
{
    return 0; 
}

// Funtion to set start point and stop point in scheduling FCFS because FCFS do not interrupt
const scheduler getFcfs()
{
    scheduler scheduler = { fcfsWhatToStart, fcfsWhetherToStop, NULL };
    return scheduler;
}

// Funtion to find earliest arrived process in scheduling rund-robin
process* roundRobinWhatToStart(const context* ctx, void* data) 
{
    process* earliestArrived = ctx->readyQueue.head->process;
    for (processNode* node = ctx->readyQueue.head->next; node != NULL; node = node->next) // check all ready queue's processes
    {
        if (node->process->notRunningSince < earliestArrived->notRunningSince) // find earliest arrived process
        {
            earliestArrived = node->process;
        }
    }
    return earliestArrived;
}

// Funtion to boolean process have to finished in time quantum in scheduling rund-robin
int roundRobinWhetherToStop(const context* ctx, void* data)
{
    uint64_t timeQuantum = *(uint64_t*)data;
    return ctx->elapsedTime - ctx->currentProcessStartTime + 1 > timeQuantum;
}

//Funtion to set scheduler in scheduling rund-robin
const scheduler getRoundRobin(uint64_t* timeQuantum)
{
    scheduler scheduler = { roundRobinWhatToStart, roundRobinWhetherToStop, timeQuantum };
    return scheduler;
}

// Funtion calculatePriority to calculate priority
double calculatePriority(const context* ctx, const process* process, double agingMultiplier) {
    uint64_t waitingTime = (ctx->elapsedTime - process->arrivalTime) - (process->burstTime - process->remainingBurstTime);
    if (process == ctx->currentProcess)
    {
        waitingTime -= (ctx->elapsedTime - ctx->currentProcessStartTime);
    }
    return process->priority + agingMultiplier * waitingTime;
}

// Funtion to highest priority process in priority aging
process* priorityAgingWhatToStart(const context* ctx, void* data)
{
    double agingMultiplier = *(double*)data;
    process* highestPriorityProcess = ctx->readyQueue.head->process;
    double highestPriority = calculatePriority(ctx, highestPriorityProcess, agingMultiplier); // calulate priority first process in ready queue

    for (processNode* node = ctx->readyQueue.head->next; node != NULL; node = node->next) // check all process in ready queue
    {
        double priority = calculatePriority(ctx, node->process, agingMultiplier);
        if (priority > highestPriority) //if find higher priority process
        {
            highestPriority = priority;
            highestPriorityProcess = node->process;
        }
    }
    return highestPriorityProcess;
}

// Funtion to check another process becomes more important in priority aging
int priorityAgingWhetherToStop(const context* ctx, void* data) 
{
    double agingMultiplier = *(double*)data;
    double currentProcessPriority = calculatePriority(ctx, ctx->currentProcess, agingMultiplier);

    for (processNode* node = ctx->readyQueue.head->next; node != NULL; node = node->next) // check all process in ready queue
    {
        double priority = calculatePriority(ctx, node->process, agingMultiplier);
        if (priority > currentProcessPriority) //If another process becomes more important during process execution
        {
            return 1;
        }
    }
    return 0;
}

// Funtion to another process aging during the process execution in priority aging
const scheduler getPriorityAging(double* agingMultiplier) 
{
    scheduler scheduler = { priorityAgingWhatToStart, priorityAgingWhetherToStop, agingMultiplier };
    return scheduler;
}

// Funtion PrintStats to print output data to output FILE
void printStats(const stats* stats, FILE* output_file) 
{
    fprintf(output_file, "Average CPU usage : %lf %% \n", stats->averageCpuUsage);
    fprintf(output_file, "Average waiting time : %lf ms\n", stats->averageWaitingTime);
    fprintf(output_file, "Average response time : %lf ms\n", stats->averageResponseTime);
    fprintf(output_file, "Average turnaround time : %lf ms\n", stats->averageTurnaroundTime);
}

/*Funtion parseInput to parse from inputed FILE data to process*/
size_t parseInput(FILE* input_file, process** jobs)
{
    *jobs = malloc(sizeof(process) * 10);
    size_t count = 0;
    while (!feof(input_file)) //while file finished
    {
        process* process = &(*jobs)[count++];
        fscanf(input_file, "%" SCNu8 "%" SCNu8 "%" SCNu64 "%" SCNu64 "\n", &process->pid, &process->priority, &process->arrivalTime, &process->burstTime);
        process->initialRunningTime = UINT64_MAX;
        process->notRunningSince = 0;
        process->remainingBurstTime = process->burstTime;
    }
    return count; // return the process array job's index size
}

/*Funrtion simulateScheduling to semi-main function for single scheduling */
void simulateScheduling(const char* title, const scheduler* scheduler, size_t jobCount, const process jobs[], FILE* output_file)
{
    fprintf(output_file, "===========================================\n");
    fprintf(output_file, "Scheduling: %s\n", title);
    fprintf(output_file, "-------------------------------------------\n");

    dispatcher dispatcher = makeDispatcher(makeContext(jobCount, jobs), scheduler); // make dispatcher variable to context switching, save scheduler data and size of jobs

    processQueue finishedProcesses = run(&dispatcher, output_file); //run all processess and take finished processes queue and they have datas.

    stats stats = { 0, 0, 0, 0 };
    uint64_t totalExecutionTime = 0;

    for (processNode* node = finishedProcesses.head; node != NULL;node = node->next) // until all processes in the finishedProcesses queue are finished
    {
        process* process = node->process;
        if (totalExecutionTime < process->notRunningSince) // update totalExecution time according to the progress
        {
            totalExecutionTime = process->notRunningSince;
        }

        double turnaroundTime = process->notRunningSince + process->burstTime;
        double waitingTime = turnaroundTime - process->arrivalTime;
        double responseTime = process->initialRunningTime - process->arrivalTime;
        stats.averageCpuUsage += process->burstTime;
        stats.averageWaitingTime += waitingTime;
        stats.averageResponseTime += responseTime;
        stats.averageTurnaroundTime += turnaroundTime;
        free(node->process);
    }
    clear(&finishedProcesses); //clear finishedProcesses queue for recycle

    stats.averageCpuUsage = stats.averageCpuUsage * 100 / totalExecutionTime;
    stats.averageWaitingTime /= jobCount;
    stats.averageResponseTime /= jobCount;
    stats.averageTurnaroundTime /= jobCount;

    fprintf(output_file, "-------------------------------------------\n");
    printStats(&stats, output_file); //printf all output data to output FILE
    fprintf(output_file, "===========================================\n\n");
}

/*Funtion main to call major funtions*/
int main(int argc, char* argv[])
{
    if (argc != 5) // defensive coding if inpus is not 5
    {
        fprintf(stderr, "%s [input_filename] [output_filename] [time_quantum_for_RR] [alpha_for_PRIO]\n",argv[0]);
    }
    else
    {
        FILE* input_file = fopen(argv[1], "r");
        FILE* output_file = fopen(argv[2], "w");
        uint64_t timeQuantum = (uint64_t)atoll(argv[3]);
        double agingMultiplier = atof(argv[4]);

        scheduler fcfs = getFcfs(); //FCFS don't need args
        scheduler roundRobin = getRoundRobin(&timeQuantum);
        scheduler priorityAging = getPriorityAging(&agingMultiplier);

        process* jobs;
        size_t jobCount = parseInput(input_file, &jobs); // call parseInput funtion to take process size

        simulateScheduling("FCFS", &fcfs, jobCount, jobs, output_file);
        simulateScheduling("Round robin", &roundRobin, jobCount, jobs, output_file);
        simulateScheduling("Preemptive priority with aging", &priorityAging, jobCount, jobs, output_file);

        free(jobs);

        fclose(input_file);
        fclose(output_file);
    }
}


