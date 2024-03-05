#include "shell.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

#define SHELL_FRAMES (FRAMESIZE / LINES_PER_FRAME)
#define SHELL_VARS (VARMEMSIZE)


struct memory_struct{
	char *var;
	char *value;
};

struct frame_struct {
    char *lines[LINES_PER_FRAME];
    int pid;
    bool empty;
	int lru;
};

int num_lru = 0;
struct memory_struct shellvars[SHELL_VARS];
struct frame_struct shellframes[SHELL_FRAMES]; 

struct memory_struct shellmemory[SHELL_VARS + SHELL_FRAMES];

// Helper functions
int match(char *model, char *var) {
	int i, len=strlen(var), matchCount=0;
	for(i=0;i<len;i++)
		if (*(model+i) == *(var+i)) matchCount++;
	if (matchCount == len)
		return 1;
	else
		return 0;
}

char *extract(char *model) {
	char token='=';    // look for this to find value
	char value[1000];  // stores the extract value
	int i,j, len=strlen(model);
	for(i=0;i<len && *(model+i)!=token;i++); // loop till we get there
	// extract the value
	for(i=i+1,j=0;i<len;i++,j++) value[j]=*(model+i);
	value[j]='\0';
	return strdup(value);
}


int resetmem() {
    for (int i = 0; i < SHELL_VARS; i++) {
        if (shellvars[i].var != NULL) {
            free(shellvars[i].var);
            shellvars[i].var = strdup("none");
        }
        if (shellvars[i].value != NULL) {
            free(shellvars[i].value);
            shellvars[i].value = strdup("none");
        }
    }
    return 0;
}


// Shell memory functions

void mem_init() {
    for (int i = 0; i < SHELL_VARS; i++) {
        shellvars[i].var = NULL;
        shellvars[i].value = NULL;
    }

    for (int i = 0; i < SHELL_FRAMES; ++i) {
        memset(shellframes[i].lines, 0, LINES_PER_FRAME * sizeof(char*));
        shellframes[i].pid = -1;
        shellframes[i].lru = -1;
        shellframes[i].empty = true;
    }
}


// Set key value pair
void mem_set_value(char *var_in, char *value_in) {
	int i=0;
    for (i = 0; i < SHELL_VARS; i++) {
        if (shellvars[i].var && strcmp(var_in, shellvars[i].var) == 0) {
            shellvars[i].value = strdup(value_in);
            return;
        }
    }

    // Value does not exist, need to find a free spot.
    for (i = 0; i < SHELL_VARS; i++) {
        if (shellvars[i].var == NULL) {
            shellvars[i].var = strdup(var_in);
            shellvars[i].value = strdup(value_in);
            return;
        }
    }

    return;
}

//get value based on input key
char *mem_get_value(char *var_in) {
	int i=0;
	for (i=0; i<SHELL_VARS; i++){
		if (strcmp(shellvars[i].var, var_in) == 0){
			return strdup(shellvars[i].value);
		} 
	}
	return NULL;
}


void printShellMemory(){
	int count_empty = 0;
	for (int i = 0; i < SHELL_VARS + SHELL_FRAMES; i++){
		if(strcmp(shellmemory[i].var,"none") == 0){
			count_empty++;
		}
		else{
			printf("\nline %d: key: %s\t\tvalue: %s\n", i, shellmemory[i].var, shellmemory[i].value);
		}
    }
	printf("\n\t%d lines in total, %d lines in use, %d lines free\n\n", SHELL_VARS + SHELL_FRAMES, SHELL_VARS + SHELL_FRAMES-count_empty, count_empty);
}

void printFrame(int frame){
	for(int i=0; i<LINES_PER_FRAME; i++){
		printf("%s\n",shellframes[frame].lines[0]);
	}
}

void printFrameStore(){
	int count_empty = 0;
	for (int i = 0; i < SHELL_FRAMES; i++){
		if(shellframes[i].empty!=0) {
			count_empty++;
		}
		else{
			printf("key: %d\tvalue: %s, %s, %s\n", i, shellframes[i].lines[0], shellframes[i].lines[1], shellframes[i].lines[2]);
			fflush(stdout);
		}
    }
	printf("\n\t%d frames in total, %d frames in use, %d frames free\n\n", SHELL_FRAMES, SHELL_FRAMES-count_empty, count_empty);
	fflush(stdout);
}

//checks frame store and returns the index of the first empty frame 
int findEmptyFrame() {
	int emptyFrame = -1;
	for(int i=0; i<SHELL_FRAMES; i++){
		if(shellframes[i].empty) {
			emptyFrame = i;
			break;
		}
	}
	return emptyFrame;
}

char* removeWhitespace(char* input){
	int i=0;
	char buffer[1000];
	while(input[i] == ' ' || input[i] == '\t' || input[i] == '\n'){
		i++;
	}
	int x=0;
	for(int j=i; input[j]!='\0'; j++){
		buffer[x] = input[j];
		x++;
	}
	buffer[x] = '\0';
	input = buffer;
	return input;
}

//loads lines from file into frame store and returns the frame number
int loadFrame(FILE* fp, int pid){
	int emptyFrame = -1;
	emptyFrame = findEmptyFrame();
	if (emptyFrame == -1){
		return emptyFrame;
	}
	//printf("Inside loadFrame():\n");
	char command[1000];
	for(int i=0; i<LINES_PER_FRAME; i++){
		if(readNextCommand(fp, command)){
			shellframes[emptyFrame].lines[i] = strdup(removeWhitespace(command));
			//printf("%s\n",trimmedCommand);
		}
		else{
			shellframes[emptyFrame].lines[i] = NULL;
		}
	}
	// for(int i=0; i<LINES_PER_FRAME; i++){
	// 	printf("%s\n",shellframes[emptyFrame].lines[i]);
	// }
	shellframes[emptyFrame].pid = pid;
	shellframes[emptyFrame].empty = false;
	return emptyFrame;
}

/*
 * Function:  addFileToMem 
 * 	Added in A2
 * --------------------
 * Load the source code of the file fp into the shell memory:
 * 		Loading format - var stores fileID, value stores a line
 *		Note that the first 100 lines are for set command, the rests are for run and exec command
 *
 *  pStart: This function will store the first line of the loaded file 
 * 			in shell memory in here
 *	pEnd: This function will store the last line of the loaded file 
 			in shell memory in here
 *  fileID: Input that need to provide when calling the function, 
 			stores the ID of the file
 * 
 * returns: error code, 21: no space left
 */
int load_file(FILE* fp, int* pStart, int* pEnd, char* filename)
{
	char *line;
    size_t i;
    int error_code = 0;
	bool hasSpaceLeft = false;
	bool flag = true;
	i=101;
	size_t candidate;
	while(flag){
		flag = false;
		for (i; i < SHELL_VARS + SHELL_FRAMES; i++){
			if(strcmp(shellmemory[i].var,"none") == 0){
				*pStart = (int)i;
				hasSpaceLeft = true;
				break;
			}
		}
		candidate = i;
		for(i; i < SHELL_VARS + SHELL_FRAMES; i++){
			if(strcmp(shellmemory[i].var,"none") != 0){
				flag = true;
				break;
			}
		}
	}
	i = candidate;
	//shell memory is full
	if(hasSpaceLeft == 0){
		error_code = 21;
		return error_code;
	}
    
    for (size_t j = i; j < SHELL_VARS + SHELL_FRAMES; j++){
        if(feof(fp))
        {
            *pEnd = (int)j-1;
            break;
        }else{
			line = calloc(1, SHELL_VARS + SHELL_FRAMES);
			if (fgets(line, SHELL_VARS + SHELL_FRAMES, fp) == NULL)
			{
				continue;
			}
			shellmemory[j].var = strdup(filename);
            shellmemory[j].value = strndup(line, strlen(line));
			free(line);
        }
    }

	//no space left to load the entire file into shell memory
	if(!feof(fp)){
		error_code = 21;
		//clean up the file in memory
		for(int j = 1; i <= SHELL_VARS + SHELL_FRAMES; i ++){
			shellmemory[j].var = "none";
			shellmemory[j].value = "none";
    	}
		return error_code;
	}
	//printShellMemory();
    return error_code;
}

char* getLineFromFrameStore(int pid, int frameIndex, int lineIndex){
	if(shellframes[frameIndex].pid==pid){
		return shellframes[frameIndex].lines[lineIndex];
	}
	return NULL;
}


char * mem_get_value_at_line(int index){
	if(index<0 || index > SHELL_VARS + SHELL_FRAMES) return NULL; 
	return shellmemory[index].value;
}


void mem_free_lines_between(int start, int end){
	for (int i=start; i<=end && i<SHELL_VARS + SHELL_FRAMES; i++){
		if(shellmemory[i].var != NULL){
			free(shellmemory[i].var);
		}	
		if(shellmemory[i].value != NULL){
			free(shellmemory[i].value);
		}	
		shellmemory[i].var = "none";
		shellmemory[i].value = "none";
	}
}
