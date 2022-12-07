#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100

pthread_mutex_t counters_lockers[MAX_NUM_COUNTERS];
pthread_mutex_t threads_lockers[MAX_NUM_THREADS];
pthread_cond_t cond_workers[MAX_NUM_THREADS];

void *worker_thread(void* thread_num){
    pthread_mutex_lock(threads_lockers[i]);
}

int main (int argc, char **argv) {
    FILE* cmdfile;
    int log_enabled, num_threads, num_counters, i;
    char counter_filename[30];
    if (argc != 5){
        printf("Bad number of arguments, exit.\n");
        return 1;
    }
    cmdfile=fopen(argv[1], "r");
    num_threads=atoi(argv[2]);
    num_counters=atoi(argv[3]);
    log_enabled=atoi(argv[4]);
    pthread_t threads[num_threads];
    FILE* counters[num_counters];
    for (i=0; i<num_counters; i++){
        sprintf(counter_filename, "./count%02d.txt", i);
        counters[i] = fopen(counter_filename ,"w");        
        fprintf(counters[i], "0");
    }
    for (i=0; i<num_threads; i++){
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }
    for (i=0; i<num_counters; i++){
        pthread_mutex_init(&counters_lockers[i], NULL);
    }
    for (i=0; i<num_threads; i++){
        pthread_mutex_init(&threads_lockers[i], NULL);
    }
    for (i=0; i<num_threads; i++){
        pthread_cond_init(&cond_workers[i], NULL);
    }
    
    // read file and exec
    for (i=0; i<num_threads; i++){
        pthread_cond_destroy(&cond_workers[i]);
    }
    for (i=0; i<num_threads; i++){
        pthread_mutex_destroy(&threads_lockers[i]);
    }
    for (i=0; i<num_counters; i++){
        pthread_mutex_destroy(&counters_lockers[i]);
    }
    for(i=0; i<num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    for (i=0; i<num_counters; i++){
        fclose(counters[i]);
    }
    fclose(cmdfile);
}