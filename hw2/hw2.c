#include "worker.h"

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
            while ((num_running_threads!=0) || (get_queue_head()!=NULL)){
                pthread_cond_wait(&cond_dispatcher_wait, &threads_mutex);
            }
            pthread_mutex_unlock(&threads_mutex);
            free(line);
        }
        else if (!strcmp(mode_str, "worker")){
            worker_jobs_num++;
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
    if (dispatcher_log!=NULL){
        fclose(dispatcher_log);
    }
    create_stats_file(worker_jobs_num);
    return 0;
}
