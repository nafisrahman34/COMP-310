// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#include <stdio.h>
#ifndef SHELL_H
#define SHELL_H
int parseInput(char *ui);
int readNextCommand(FILE *fp, char *command);
#endif

#ifndef FRAMESIZE
#define FRAMESIZE 99
#endif

#ifndef VARMEMSIZE
#define VARMEMSIZE 20
#endif

#define LINES_PER_FRAME 3