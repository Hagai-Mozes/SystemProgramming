#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

#include "queue.h"

/* constants */
#define MAX_LINE_SIZE 1025
#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define ACTION_MSLEEP 0

/* global locks and variables */
pthread_mutex_t counters_mutex[MAX_NUM_COUNTERS];
pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_threads = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_dispatcher_wait = PTHREAD_COND_INITIALIZER;

/* local structs */
typedef enum log_mode{
    LOG_START,
    LOG_END,
    LOG_READ
} Log_mode;

struct thread_data_s{
    int id;
};

typedef struct file_s{
    FILE* fp;
    char name[30];
} File_s;

/* global variables */
int finish_flag;
int num_running_threads=0;
int log_enabled=0;
long long int start_time;
long long int sum_jobs_time=0;
long long int min_jobs_time=LLONG_MAX;
long long int max_jobs_time=0;

/* global arrays */
File_s counters[MAX_NUM_COUNTERS];
struct thread_data_s thread_data[MAX_NUM_THREADS];

/* write to line to a log file according to mode */
void write_log_line(FILE* log_file, char *line, Log_mode mode){
    if(log_enabled){
        long long int end_time;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        end_time = tv.tv_sec*1000 + tv.tv_usec/1000;
        if(line[strlen(line)-1] != '\n'){
            line[strlen(line)] = '\n';
            line[strlen(line)+1] = '\0';
        }
        if (mode==LOG_START){
            fprintf(log_file, "TIME %lld: START job %s", end_time - start_time, line);
        }
        else if(mode==LOG_END){
            fprintf(log_file, "TIME %lld: END job %s", end_time - start_time, line);
        }
        else{
            fprintf(log_file, "TIME %lld: read cmd line: %s", end_time - start_time, line);
        }
    }
}

/* update worker statistics */
long long int update_calc(){
    long long int end_time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    end_time = tv.tv_sec*1000 + tv.tv_usec/1000;
    sum_jobs_time += (end_time - start_time);
    if(end_time - start_time < min_jobs_time){
        min_jobs_time = end_time - start_time;
    }
    if(max_jobs_time < end_time - start_time){
        max_jobs_time = end_time - start_time;
    }
}

/* get a worker line string and return linked list of tasks for a worker */
Counter_args_s *parse_worker_line(char* line){
    int i=0, repeat=1;
    Counter_args_s *counter_args ,*head_counter_args;
    char *worker_cmd_str;
    head_counter_args=NULL;
    counter_args=NULL;
    while((worker_cmd_str = strtok(NULL, " ;\n")) != NULL){
        if(!strcmp(worker_cmd_str,"repeat")){
            repeat=atoi(strtok(NULL, " ;\n"));
            continue;
        }
        if(counter_args==NULL){
            counter_args = (Counter_args_s*) malloc(sizeof(Counter_args_s));
            head_counter_args=counter_args;
        }
        else{
            counter_args->next = (Counter_args_s*) malloc(sizeof(Counter_args_s));
            counter_args=counter_args->next;
        }
        counter_args->cmd_num = atoi(strtok(NULL, " ;\n"));
        if(!strcmp(worker_cmd_str,"msleep")){
            counter_args->counter_action = ACTION_MSLEEP;
            counter_args->cmd_num *= repeat;
        }
        else if(!strcmp(worker_cmd_str,"increment")){
            counter_args->counter_action = 1*repeat;
        }
        else if(!strcmp(worker_cmd_str,"decrement")){
            counter_args->counter_action = -1*repeat;
        }
        counter_args->next=NULL;
    }
    return head_counter_args;
}

void action_on_counter(Counter_args_s* curr_counter_args){
    long long int counter_val;
    File_s file_s = counters[curr_counter_args->cmd_num];
    // increment and decrement requires reading and writing to counter file
    // lock the file until counter update is done
    pthread_mutex_lock(&counters_mutex[curr_counter_args->cmd_num]);
    counter_val=0;
    // need to open and close the counter in order to save the data and set cursor back in the start
    file_s.fp=fopen(file_s.name, "r");
    fscanf(file_s.fp, "%lld", &counter_val);
    fclose(file_s.fp);
    // counter_action=-1 for decrement, counter_action=1 for increment, 0 for msleep
    counter_val+=curr_counter_args->counter_action;
    file_s.fp=fopen(file_s.name, "w");
    fprintf(file_s.fp, "%lld", counter_val);
    fclose(file_s.fp);
    pthread_mutex_unlock(&counters_mutex[curr_counter_args->cmd_num]);
}

/* function that a each thread runs */
void *worker_thread(void* arg){
    num_running_threads+=1;
    struct thread_data_s *thread_data = (struct thread_data_s *) arg;
    Counter_args_s *curr_counter_args=NULL, *prev_counter_args=NULL;
    Jobs *curr_job=NULL;
    int id = thread_data->id;
    char log_file_name[15];
    FILE* log_file;
    if (log_enabled==1){
        sprintf(log_file_name, "thread%02d.txt", id);
        log_file=fopen(log_file_name, "w");
    }
    while(1){
        pthread_mutex_lock(&threads_mutex);
        // lock the queue from reading to dequeue - no other thread can dequeue meanwhile
        pthread_mutex_lock(&queue_lock);
        // if queue is empty and reading the cmd file not finished yes - wait on condition
        while (get_queue_head()==NULL && finish_flag == 0){
            num_running_threads-=1;
            // send signal to dispatcher_wait to indicate that all commands finished - with num_running_threads
            pthread_cond_signal(&cond_dispatcher_wait);
            pthread_mutex_unlock(&queue_lock);
            pthread_cond_wait(&cond_threads, &threads_mutex);
            num_running_threads+=1;
        }
        pthread_mutex_unlock(&threads_mutex);
        if (get_queue_head()==NULL && (finish_flag == 1)){
            pthread_mutex_unlock(&queue_lock);
            break;
        }
        curr_job=dequeue();
        curr_counter_args=curr_job->counter_args_head;
        pthread_mutex_unlock(&queue_lock);
        write_log_line(log_file, curr_job->line, LOG_START);
        // execute the tasks in the linked list
        while (curr_counter_args!=NULL){
            if(curr_counter_args->counter_action == ACTION_MSLEEP){
                usleep((curr_counter_args->cmd_num)*1000);
            }
            else{
                action_on_counter(curr_counter_args);
            }
            prev_counter_args=curr_counter_args;
            curr_counter_args=curr_counter_args->next;
            free(prev_counter_args);
        }
        write_log_line(log_file, curr_job->line, LOG_END);
        update_calc();
        free(curr_job->line);
        free(curr_job);
    }
    if (log_enabled==1){
        fclose(log_file);
    }
    num_running_threads-=1;
}

void create_stats_file(int worker_jobs_num){
    long long int end_time;
    struct timeval tv;
    FILE* stats;
    gettimeofday(&tv, NULL);
    end_time = tv.tv_sec*1000 + tv.tv_usec/1000;
    stats = fopen("stats.txt", "w");
    fprintf(stats,"total running time: %lld milliseconds\n", end_time - start_time);
    fprintf(stats,"sum of jobs turnaround time: %lld milliseconds\n", sum_jobs_time);
    fprintf(stats,"min job turnaround time: %lld milliseconds\n", min_jobs_time);
    fprintf(stats,"average job turnaround time: %lld milliseconds\n", sum_jobs_time/worker_jobs_num);
    fprintf(stats,"max job turnaround time: %lld milliseconds\n", max_jobs_time);
    fclose(stats);
}

int main (int argc, char **argv) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    start_time = tv.tv_sec*1000 + tv.tv_usec/1000;
    FILE* cmdfile, *dispatcher_log, *stats;
    char *line, *line_returned_buffer;
    char line_buffer[MAX_LINE_SIZE];
    int num_threads, num_counters, i, worker_jobs_num=0;
    Counter_args_s *head_counter_args;
    int repeat=1;
    char* mode_str, *worker_cmd_str;
    unsigned int sleep_time;
    if (argc != 5){
        printf("Bad number of arguments, exit.\n");
        return 1;
    }
    cmdfile=fopen(argv[1], "r");
    num_threads=atoi(argv[2]);
    num_counters=atoi(argv[3]);
    log_enabled=atoi(argv[4]);
    pthread_mutex_init(&threads_mutex, NULL);
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&cond_threads, NULL);
    pthread_cond_init(&cond_dispatcher_wait, NULL);
    min_jobs_time=LLONG_MAX;
    sum_jobs_time=0;

    pthread_t threads[num_threads];
    //create dispatcher log file if needed
    if(log_enabled){
        dispatcher_log=fopen("dispatcher.txt", "w");
    }
    // create all counters files
    for (i=0; i<num_counters; i++){
        sprintf(counters[i].name, "./count%02d.txt", i);
        counters[i].fp = fopen(counters[i].name, "w"); 
        fprintf(counters[i].fp, "0");
        fclose(counters[i].fp);
    }
    // create all threads
    for (i=0; i<num_threads; i++){
        thread_data[i].id=i;
        pthread_create(&threads[i], NULL, worker_thread, (void*) &thread_data[i]);
    }
    // create all counters lockers
    for (i=0; i<num_counters; i++){
        pthread_mutex_init(&counters_mutex[i], NULL);
    }
    // read commands file and exec
    line=(char*) malloc(MAX_LINE_SIZE * sizeof(char));
    line_returned_buffer="enter to the loop";
    while((line_returned_buffer=fgets(line, MAX_LINE_SIZE, cmdfile)) != NULL){
        write_log_line(dispatcher_log, line, LOG_READ);
        strcpy(line_buffer, line);
        mode_str = strtok(line_buffer, " ;\n");
        if (!strcmp(mode_str, "dispatcher_msleep")){
            sleep_time = atoi(strtok(NULL, " ;\n"));
            usleep(sleep_time*1000);
            free(line);
        }
        else if (!strcmp(mode_str, "dispatcher_wait")){
            pthread_mutex_lock(&threads_mutex);
            pthread_mutex_lock(&queue_lock);
            while ((num_running_threads!=0) || (get_queue_head()!=NULL)){
                pthread_mutex_unlock(&queue_lock);
                pthread_cond_wait(&cond_dispatcher_wait, &threads_mutex);
            }
            pthread_mutex_unlock(&queue_lock);
            pthread_mutex_unlock(&threads_mutex);
            free(line);
        }
        else if (!strcmp(mode_str, "worker")){
            worker_jobs_num++;
            head_counter_args=parse_worker_line(line_buffer);
            pthread_mutex_lock(&queue_lock);
            enqueue(head_counter_args, line);
            pthread_mutex_unlock(&queue_lock);
            pthread_cond_signal(&cond_threads);
        }
        else{
            fprintf(stderr, "Illegal command: %s\n", line);
        }
        line=(char*) malloc(MAX_LINE_SIZE * sizeof(char));
    }
    // finished reading commands file
    finish_flag=1;
    // broadcasting to finish waiting threads
    pthread_cond_broadcast(&cond_threads);
    for(i=0; i<num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    pthread_cond_destroy(&cond_threads);
    pthread_cond_destroy(&cond_dispatcher_wait);
    pthread_mutex_destroy(&threads_mutex);
    pthread_mutex_destroy(&queue_lock);
    for (i=0; i<num_counters; i++){
        pthread_mutex_destroy(&counters_mutex[i]);
    }
    fclose(cmdfile);
    if (log_enabled){
        fclose(dispatcher_log);
    }
    create_stats_file(worker_jobs_num);
    return 0;
}
