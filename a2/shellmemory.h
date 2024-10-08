// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#ifndef SHELLMEMORY_H
#define SHELLMEMORY_H
void mem_init();
char *mem_get_value(char *var);
void mem_set_value(char *var, char *value);
int load_file(FILE* fp, int* pStart, int* pEnd, char* fileID);
char * mem_get_value_at_line(int index);
void mem_free_lines_between(int start, int end);
void printShellMemory();
int loadFrame(FILE* fp, int pid);
void printFrameStore();
void printFrame(int frame);
char* getLineFromFrameStore(int pid, int frameIndex, int lineIndex);
#endif