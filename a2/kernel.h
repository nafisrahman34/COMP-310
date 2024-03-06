// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#ifndef KERNEL
#define KERNEL
#include "pcb.h"
int process_initialize(char *filename, int id);
int schedule_by_policy(char* policy); //, bool mt);
int shell_process_initialize();
void ready_queue_destroy();
#endif