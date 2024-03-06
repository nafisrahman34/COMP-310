// Names: Nafis Rahman, Brandon Fook Chong
// IDs: 260889422, 260989601
#ifndef INTERPRETER_H
#define INTERPRETER_H

int interpreter(char* command_args[], int args_size);
int help();

enum Error {
	NO_ERROR,
	FILE_DOES_NOT_EXIST,
	FILE_ERROR,
	NO_MEM_SPACE,
	READY_QUEUE_FULL,
	SCHEDULING_ERROR,
	TOO_MANY_TOKENS,
	TOO_FEW_TOKENS,
	NON_ALPHANUMERIC_TOKEN,
	BAD_COMMAND,
	ERROR_CD,
	ERROR_MKDIR,
	ERROR_LOADFRAME,
};

#endif