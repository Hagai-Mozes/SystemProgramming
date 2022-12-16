#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>

/* structs */
typedef struct counter_args_s{
    int cmd_num;
    int counter_action; //-1 = decrement, 0 = sleep, 1 = increment
    struct counter_args_s *next;
} Counter_args_s;

typedef struct jobs{
    Counter_args_s* counter_args_head;
    char *line;
    struct jobs *next;
} Jobs;

/* functions */
void print_counter_args(Counter_args_s* head);
Jobs* dequeue();
Jobs* enqueue(Counter_args_s* job, char* line);
void free_linked_list(Counter_args_s* head);
Jobs* get_queue_head();
Jobs* get_queue_tail();

#endif