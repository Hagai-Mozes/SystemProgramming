#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
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

/* structs */
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
static int finish_flag;
static int log_enabled;
static int num_running_threads;
static long long int start_time;
static long long int sum_jobs_time;
static long long int min_jobs_time;
static long long int max_jobs_time;

/* global arrays */
static File_s counters[MAX_NUM_COUNTERS];
static struct thread_data_s thread_data[MAX_NUM_THREADS];

/* global locks and variables */
static pthread_mutex_t counters_mutex[MAX_NUM_COUNTERS];
static pthread_mutex_t threads_mutex;
static pthread_mutex_t queue_lock;
static pthread_cond_t cond_threads;
static pthread_cond_t cond_dispatcher_wait;

/* functions */
void *worker_thread(void* arg);
void action_on_counter(Counter_args_s* curr_counter_args);
Counter_args_s *parse_worker_line(char* line);
void create_stats_file(int worker_jobs_num);
void write_log_line(FILE* log_file, char *line, Log_mode mode);

#endif