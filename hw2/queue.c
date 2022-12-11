#include "queue.h"

Jobs *jobs_head = NULL;
Jobs *jobs_tail=NULL;

Jobs* get_queue_head(){
    return jobs_head;
}

Jobs* get_queue_tail(){
    return jobs_tail;
}

Jobs* dequeue(){
    Jobs* curr_jobs_head;
    curr_jobs_head=jobs_head;
    jobs_head=jobs_head->next;
    if(jobs_head==NULL){
        jobs_tail = NULL;
    }
    return curr_jobs_head;
}

Jobs* enqueue(Counter_args_s* job, char* line){
    Jobs* new_job;
    new_job = (Jobs*) malloc(sizeof(Jobs));
    new_job->counter_args_head = job;
    new_job->line=line;
    new_job->next=NULL;
    if(jobs_tail==NULL){
        jobs_tail = new_job;
        jobs_head=jobs_tail;
    }
    else{
        jobs_tail->next=new_job;
        jobs_tail=jobs_tail->next;
    }
    return jobs_tail;
}

void free_linked_list(Counter_args_s* head){
    Counter_args_s *curr, *prev;
    curr = head;
    while(curr != NULL){
        prev = curr;
        curr = curr->next;
        free(prev);
    }
}

void repeat_commands(Counter_args_s *head_copy_counter_args, Counter_args_s *tail_copy_counter, int repeat){
    int i;
    Counter_args_s *copy_counter_args, *counter_args;
    counter_args=tail_copy_counter;
    for (i=0; i < repeat-1; i++){
        copy_counter_args = head_copy_counter_args;
        while (copy_counter_args != tail_copy_counter->next){
            counter_args->next = (Counter_args_s*) malloc(sizeof(Counter_args_s));
            counter_args=counter_args->next;
            counter_args->counter_action = copy_counter_args->counter_action;
            counter_args->cmd_num = copy_counter_args->cmd_num;
            counter_args->next=NULL;
            copy_counter_args = copy_counter_args->next;
        }
    }
}