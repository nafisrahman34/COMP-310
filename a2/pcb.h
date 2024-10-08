// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#ifndef PCB_H
#define PCB_H
#include <stdbool.h>
/*
 * Struct:  PCB 
 * --------------------
 * pid: process(task) id
 * PC: program counter, stores the index of line that the task is executing
 * start: the first line in shell memory that belongs to this task
 * end: the last line in shell memory that belongs to this task
 * job_length_score: for EXEC AGING use only, stores the job length score
 */
typedef struct
{
    int page_table[100];
    bool priority;
    int pid;
    int PC;
    int current_command;
    int number_of_commands; 
    int end;
    int job_length_score;
    FILE *fp;
    int start; 
}PCB;

void freePCB(PCB *pcb);
void setFrame(PCB *pcb, int frame_index, int frame);
int generatePID();
PCB* makePCB(FILE *fp, int num_commands);

#endif