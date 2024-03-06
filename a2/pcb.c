// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pcb.h"

static int pid_counter = 1;

int generatePID() {
    return pid_counter++;
}

PCB* makePCB(FILE *fp, int num_commands) {
    PCB *newPCB = malloc(sizeof(PCB));
    if (newPCB == NULL) {
        perror("Error: Unable to allocate memory for PCB");
        exit(EXIT_FAILURE);
    }

    newPCB->fp = fp;
    newPCB->current_command = 0;
    newPCB->pid = generatePID();
    newPCB->PC = 0;
    newPCB->job_length_score = num_commands;
    newPCB->priority = false;
    newPCB->number_of_commands = num_commands;

    memset(newPCB->page_table, -1, sizeof(newPCB->page_table));

    return newPCB;
}

void freePCB(PCB *pcb) {
    if (pcb == NULL) return;

    fclose(pcb->fp);
    free(pcb);
}

void setFrame(PCB *pcb, int frame_index, int frame) {
    if (pcb == NULL || frame_index < 0 || frame_index >= 100) return;

    pcb->page_table[frame_index] = frame;
}