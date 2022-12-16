#include "worker.h"

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
    int i=0, repeat=0, repeat_flag=0;
    Counter_args_s *counter_args ,*head_counter_args, *head_copy_counter_args;
    char *worker_cmd_str;
    head_counter_args=NULL;
    counter_args=NULL;
    while((worker_cmd_str = strtok(NULL, " ;\n")) != NULL){
        if(!strcmp(worker_cmd_str,"repeat")){
            repeat=atoi(strtok(NULL, " ;\n"));
            repeat_flag=1;
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
        if(repeat_flag){
            repeat_flag=0;
            head_copy_counter_args=counter_args;
        }
        if(!strcmp(worker_cmd_str,"msleep")){
            counter_args->counter_action = ACTION_MSLEEP;
        }
        else if(!strcmp(worker_cmd_str,"increment")){
            counter_args->counter_action = 1;
        }
        else if(!strcmp(worker_cmd_str,"decrement")){
            counter_args->counter_action = -1;
        }
        counter_args->cmd_num = atoi(strtok(NULL, " ;\n")); //FIXME - does every command ends with ;?
        counter_args->next=NULL;
    }
    repeat_commands(head_copy_counter_args, counter_args, repeat);
    pthread_cond_signal(&cond_threads);
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