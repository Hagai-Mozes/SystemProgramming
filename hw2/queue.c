#include "queue.h"

Jobs *jobs_head = NULL;
Jobs *jobs_tail=NULL;

Jobs* get_queue_head(){
    return jobs_head;
}

Jobs* get_queue_tail(){
    return jobs_tail;
}

/* pop out the head return it */
Jobs* dequeue(){
    Jobs* curr_jobs_head;
    curr_jobs_head=jobs_head;
    jobs_head=jobs_head->next;
    if(jobs_head==NULL){
        jobs_tail = NULL;
    }
    return curr_jobs_head;
}

/* insert new node to the tail of the jobs queue */
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

// print linked list of tasks for a worker - for debugging
void print_counter_args(Counter_args_s* head){
    Counter_args_s* counter_args;
    int node_num=1;
    counter_args = head;
    while (counter_args!=NULL){
        printf("node_num = %d: counter_args->counter_action = %d , counter_args->cmd_num = %d\n", node_num, counter_args->counter_action, counter_args->cmd_num);
        counter_args=counter_args->next;
        node_num++;
    }
}
