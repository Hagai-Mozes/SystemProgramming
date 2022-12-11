#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "queue.h"

#define MAX_LINE_SIZE 1025
#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define ACTION_MSLEEP 0

pthread_mutex_t counters_mutex[MAX_NUM_COUNTERS];
pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_threads = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_dispatcher_wait = PTHREAD_COND_INITIALIZER;

typedef enum log_mode{
    LOG_START,
    LOG_END,
    LOG_READ
} Log_mode;

struct thread_data_s{
    int id;
};

//Global variables
long long int start_time;
int log_enabled=0;
int finish_flag;
int num_running_threads=0;

FILE* counters[MAX_NUM_COUNTERS];
struct thread_data_s thread_data[MAX_NUM_THREADS];

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

Counter_args_s *parse_worker_line(char* line){
    int i=0, repeat=0;
    Counter_args_s *counter_args ,*head_counter_args, *head_copy_counter_args;
    char *worker_cmd_str;
    head_counter_args=NULL;
    counter_args=NULL;
    while((worker_cmd_str = strtok(NULL, " ;\n")) != NULL){
        if(counter_args==NULL){
            counter_args = (Counter_args_s*) malloc(sizeof(Counter_args_s));
            head_counter_args=counter_args;
        }
        else{
            if(!strcmp(worker_cmd_str,"repeat")){
                head_copy_counter_args=counter_args;
                repeat=atoi(strtok(NULL, ";\n"));
                continue;
            }
            else {
                counter_args->next = (Counter_args_s*) malloc(sizeof(Counter_args_s));
                counter_args=counter_args->next;
            }
        }
        if(!strcmp(worker_cmd_str,"msleep")){
            counter_args->counter_action = ACTION_MSLEEP;
        }
        else if(!strcmp(worker_cmd_str,"increment")){
            counter_args->counter_action = 1;
        }
        else if(!strcmp(worker_cmd_str,"decrement")){
            printf("finish dispetcher_wait - now decrement\n");
            counter_args->counter_action = -1;
        }
        counter_args->cmd_num = atoi(strtok(NULL, " ;\n")); //FIXME - does every command ends with ;?
        counter_args->next=NULL;
    }
    repeat_commands(head_copy_counter_args->next, counter_args, repeat);
    pthread_cond_signal(&cond_threads);
    return head_counter_args;
}

void *worker_thread(void* arg){
    num_running_threads+=1;
    struct thread_data_s *thread_data = (struct thread_data_s *) arg;
    Counter_args_s *curr_counter_args=NULL, *prev_counter_args=NULL;
    Jobs *curr_job=NULL;
    char* counter_file_name;
    char value_str[30];
    int id = thread_data->id;
    long long int counter_val;
    char log_file_name[15];
    FILE* log_file;
    if (log_enabled==1){
        sprintf(log_file_name, "thread%02d.txt", id);
        log_file=fopen(log_file_name, "w");
    }
    printf("hi from %d\n", thread_data->id);
    while(1){
        pthread_mutex_lock(&threads_mutex);
        pthread_mutex_lock(&queue_lock);
        while (get_queue_head()==NULL && finish_flag == 0){
            num_running_threads-=1;
            pthread_cond_signal(&cond_dispatcher_wait);
            pthread_cond_wait(&cond_threads, &threads_mutex);
            num_running_threads+=1;
        }
        pthread_mutex_unlock(&threads_mutex);
        if (get_queue_head()==NULL && (finish_flag == 1)){
            printf("bye from %d\n", thread_data->id);
            pthread_mutex_unlock(&queue_lock);
            break;
        }
        curr_job=dequeue();
        curr_counter_args=curr_job->counter_args_head;
        pthread_mutex_unlock(&queue_lock);
        write_log_line(log_file, curr_job->line, LOG_START);
        while (curr_counter_args!=NULL){
            if(curr_counter_args->counter_action == ACTION_MSLEEP){
                usleep((curr_counter_args->cmd_num)*1000);
            }
            else{
                pthread_mutex_lock(&counters_mutex[curr_counter_args->cmd_num]);
                fscanf(counters[curr_counter_args->cmd_num], "%lld", &counter_val);
                counter_val+=curr_counter_args->counter_action;
                sprintf(value_str, "%lld", counter_val);
                counters[curr_counter_args->cmd_num]=freopen(NULL,"w",counters[curr_counter_args->cmd_num]);
                fwrite(value_str, 1, strlen(value_str), counters[curr_counter_args->cmd_num]);
                pthread_mutex_unlock(&counters_mutex[curr_counter_args->cmd_num]);
            }
            prev_counter_args=curr_counter_args;
            curr_counter_args=curr_counter_args->next;
            free(prev_counter_args);
        }
        write_log_line(log_file, curr_job->line, LOG_END);
        free(curr_job->line);
        free(curr_job);
    }
    fclose(log_file);
    num_running_threads-=1;
}

int main (int argc, char **argv) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    start_time= tv.tv_sec*1000 + tv.tv_usec/1000;
    FILE* cmdfile, *dispatcher_log;
    char *line, *line_returned_buffer;
    char line_buffer[MAX_LINE_SIZE];
    int num_threads, num_counters, i;
    char counter_filename[30];
    if (argc != 5){
        printf("Bad number of arguments, exit.\n");
        return 1;
    }
    cmdfile=fopen(argv[1], "r");
    size_t buffer_size;
    num_threads=atoi(argv[2]);
    num_counters=atoi(argv[3]);
    log_enabled=atoi(argv[4]);
    int thread_ids[num_threads], j;
    pthread_t threads[num_threads];
    Counter_args_s *head_counter_args;
    int repeat=1;
    char* mode_str, *worker_cmd_str;
    unsigned int sleep_time;
    if(log_enabled){
        dispatcher_log=fopen("dispatcher.txt", "w");
    }
    // create all counters files
    for (i=0; i<num_counters; i++){
        sprintf(counter_filename, "./count%02d.txt", i);
        counters[i] = fopen(counter_filename, "w");        
        fprintf(counters[i], "0");
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
    
    ////////////////////////////////
    // start read file and exec   //
    ////////////////////////////////

    line_returned_buffer="enter to the loop";
    line=(char*) malloc(MAX_LINE_SIZE * sizeof(char));
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
            while ((num_running_threads!=0) || (get_queue_head()!=NULL)){
                pthread_cond_wait(&cond_dispatcher_wait, &threads_mutex);
            }
            pthread_mutex_unlock(&threads_mutex);
            free(line);
        }
        else if (!strcmp(mode_str, "worker")){
            head_counter_args=parse_worker_line(line_buffer);
            pthread_mutex_lock(&queue_lock);
            enqueue(head_counter_args, line);
            pthread_mutex_unlock(&queue_lock);
        }
        else{
            fprintf(stderr, "Illegal command: %s\n", line);
        }
        line=(char*) malloc(MAX_LINE_SIZE * sizeof(char));
    }
    printf("finished read file\n");
    finish_flag=1;
    pthread_cond_broadcast(&cond_threads);
    for(i=0; i<num_threads; i++){
        pthread_join(threads[i], NULL);
        printf("pthread_join\n");
    }
    pthread_cond_destroy(&cond_threads);
    pthread_cond_destroy(&cond_dispatcher_wait);
    pthread_mutex_destroy(&threads_mutex);
    pthread_mutex_destroy(&queue_lock);
    for (i=0; i<num_counters; i++){
        pthread_mutex_destroy(&counters_mutex[i]);
    }
    for (i=0; i<num_counters; i++){
        fclose(counters[i]);
    }
    fclose(cmdfile);
    if (dispatcher_log!=NULL){
        fclose(dispatcher_log);
    }
}