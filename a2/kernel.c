#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pcb.h"
#include "kernel.h"
#include "shell.h"
#include "shellmemory.h"
#include "interpreter.h"
#include "ready_queue.h"
#include "interpreter.h"

bool active = false;
bool debug = false;
bool in_background = false;

int copy_script_to_backing_store(char *filename, int id) {
    char command[1000];
    sprintf(command, "cp %s %s/%s.%d", filename, "backing_store", filename, id);
    return system(command);
}

int process_initialize(char *filename, int id) {
    FILE *fp;
    int error_code = 0;

    fp = fopen(filename, "rt");
    if (fp == NULL) {
        return 11;
    }

    int num_commands = 0;
    char line[1000];
    //counts number of commands in the file
    while (readNextCommand(fp, line)) {
        num_commands++;
    }
    fclose(fp);

    //copies input to backing store directory
    error_code = copy_script_to_backing_store(filename, id);
    if (error_code != 0) {
        return error_code;
    }

    char new_filename[1000];
    sprintf(new_filename, "%s/%s.%d", "backing_store", filename, id);
    //opens the file from the backing store directory
    fp = fopen(new_filename, "rt");
    if (fp == NULL) {
        return 11;
    }

    PCB *pcb = makePCB(fp, num_commands);
    //find an empty frame in frame store and load upto 2 frames of data into the frame store with data from the file
    for(int i=0; i<2 && i*LINES_PER_FRAME<num_commands; i++){
        int frame = loadFrame(fp, id);
        if(frame == -1) return 12;
        pcb->current_command = 0;
        setFrame(pcb, i, frame);
    }

    QueueNode *node = malloc(sizeof(QueueNode));
    node->pcb = pcb;
    ready_queue_add_to_tail(node);
    return error_code;
}

// int shell_process_initialize(){
//     //Note that "You can assume that the # option will only be used in batch mode."
//     //So we know that the input is a file, we can directly load the file into ram
//     int* start = (int*)malloc(sizeof(int));
//     int* end = (int*)malloc(sizeof(int));
//     int error_code = 0;
//     error_code = load_file(stdin, start, end, "_SHELL");
//     if(error_code != 0){
//         return error_code;
//     }
//     PCB* newPCB = makePCB(*start,*end);
//     newPCB->priority = true;
//     QueueNode *node = malloc(sizeof(QueueNode));
//     node->pcb = newPCB;

//     ready_queue_add_to_head(node);

//     freopen("/dev/tty", "r", stdin);
//     return 0;
// }

bool execute_process(QueueNode *node, int quanta){
    char *line = NULL;
    PCB *pcb = node->pcb;
    bool output = false;
    char* pageFault = "Page fault";
    for(int i=0; i<quanta; i++){
        //check if process is finished; return true if finished
        if(pcb->PC == pcb->number_of_commands){
            terminate_process(node);
            in_background = false;
            output = true;
            break;
        }
        //find frame index and line index for current PC
        int pageIndex = pcb->PC/LINES_PER_FRAME;
        int lineIndex = pcb->PC%LINES_PER_FRAME;
        int frameIndex = pcb->page_table[pageIndex];
        //handling page faults 
        if(frameIndex==-1 || getLineFromFrameStore(pcb->pid, frameIndex, lineIndex) == NULL){
            rewind(pcb->fp);
            int i=0;
            char throwaway[1000];
            while(i!=pcb->current_command){
                i++;
                readNextCommand(pcb->fp, throwaway);
            }
            int frame = loadFrame(pcb->fp, pcb->pid);
            setFrame(pcb, pageIndex, frame);
            return output;
        }
        line = getLineFromFrameStore(pcb->pid, frameIndex, lineIndex);
        pcb->PC++;
        pcb->current_command++;
        in_background = true;
        if(pcb->priority) {
            pcb->priority = false;
        }
        parseInput(line);
        in_background = false;
    }
    return output;
}

void *scheduler_FCFS(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;   
        }
        cur = ready_queue_pop_head();
        execute_process(cur, MAX_INT);
    }
    return 0;
}

void *scheduler_SJF(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_shortest_job();
        execute_process(cur, MAX_INT);
    }
    return 0;
}

void *scheduler_AGING_alternative(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_shortest_job();
        ready_queue_decrement_job_length_score();
        if(!execute_process(cur, 1)) {
            ready_queue_add_to_head(cur);
        }   
    }
    return 0;
}

void *scheduler_AGING(){
    QueueNode *cur;
    int shortest;
    sort_ready_queue();
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_head();
        shortest = ready_queue_get_shortest_job_score();
        if(shortest < cur->pcb->job_length_score){
            ready_queue_promote(shortest);
            ready_queue_add_to_tail(cur);
            cur = ready_queue_pop_head();
        }
        ready_queue_decrement_job_length_score();
        if(!execute_process(cur, 1)) {
            ready_queue_add_to_head(cur);
        }
    }
    return 0;
}

void *scheduler_RR(void *arg){
    int quanta = ((int *) arg)[0];
    QueueNode *cur;
    while(true){
        if(is_ready_empty()){
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_head();
        if(!execute_process(cur, quanta)) {
            ready_queue_add_to_tail(cur);
        }
    }
    return 0;
}

int schedule_by_policy(char* policy){ //, bool mt){
    if(strcmp(policy, "FCFS")!=0 && strcmp(policy, "SJF")!=0 && 
        strcmp(policy, "RR")!=0 && strcmp(policy, "AGING")!=0 && strcmp(policy, "RR30")!=0){
            return SCHEDULING_ERROR;
    }
    if(active) return 0;
    if(in_background) return 0;
    int arg[1];
    if(strcmp("FCFS",policy)==0){
        scheduler_FCFS();
    }else if(strcmp("SJF",policy)==0){
        scheduler_SJF();
    }else if(strcmp("RR",policy)==0){
        arg[0] = 2;
        scheduler_RR((void *) arg);
    }else if(strcmp("AGING",policy)==0){
        scheduler_AGING();
    }else if(strcmp("RR30", policy)==0){
        arg[0] = 30;
        scheduler_RR((void *) arg);
    }
    return 0;
}

