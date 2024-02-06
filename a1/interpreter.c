#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/stat.h> 
#include "shellmemory.h"
#include "shell.h"

int MAX_ARGS_SIZE = 8;
int badcommandFileDoesNotExist();
int echo(char* var);
int help();
int my_cat(char* filename);
int my_cd(char* dirname);
int my_delete(char* filename);
int my_ls();
int my_mkdir(char *dirname);
int my_touch(char* filename);
int print(char* var);
int quit();
int run(char* script);
int set(char* var, char* value);

int badcommand(){
	printf("%s\n", "Unknown Command");
	return 1;
}

// For run command only
int badcommandFileDoesNotExist(){
	printf("%s\n", "Bad command: File not found");
	return 3;
}

// Interpret commands and their arguments
int interpreter(char* command_args[], int args_size){
	int i;

	if ( args_size < 1 || args_size > MAX_ARGS_SIZE){
		return badcommand();
	}

	for ( i=0; i<args_size; i++){ //strip spaces new line etc
		command_args[i][strcspn(command_args[i], "\r\n")] = 0;
	}

	if (strcmp(command_args[0], "help")==0){
	    //help
	    if (args_size != 1) return badcommand();
	    return help();
	
	} else if (strcmp(command_args[0], "quit")==0) {
		//quit
		if (args_size != 1) return badcommand();
		return quit();

	} else if (strcmp(command_args[0], "set")==0) {
		//set
		if (args_size < 3 || args_size > 7){
			printf("%s\n","Bad command: set");	
		}
		else{
			char value[1000] = "";
			strcpy(value, command_args[2]);
			if(args_size > 3){
				for(int i = 3; i<args_size; i++){
					strcat(value, " ");
					strcat(value, command_args[i]);
				}
			}		
			return set(command_args[1], value);
		}
	
	} else if (strcmp(command_args[0], "print")==0) {
		if (args_size != 2) return badcommand();
		return print(command_args[1]);

	} else if (strcmp(command_args[0], "echo")==0) {
		if (args_size != 2) return badcommand();
		return echo(command_args[1]);

	} else if (strcmp(command_args[0], "my_mkdir")==0) {
		if (args_size > 2) return badcommand();
		return my_mkdir(command_args[1]);

	} else if (strcmp(command_args[0], "my_ls")==0) {
		if (args_size != 1) return badcommand();
		return my_ls();

	} else if (strcmp(command_args[0], "my_cd")==0) {
		if (args_size > 2) return badcommand();
		return my_cd(command_args[1]);
		
	} else if(strcmp(command_args[0], "my_touch")==0) {
		if(args_size != 2) return badcommand();
		return my_touch(command_args[1]);

	} else if(strcmp(command_args[0], "my_cat")==0) {
		if(args_size != 2) return badcommand();
		return my_cat(command_args[1]);

	} else if(strcmp(command_args[0], "my_delete")==0) {
		if(args_size != 2) return badcommand();
		return my_delete(command_args[1]);

	} else if (strcmp(command_args[0], "run")==0) {
		if (args_size != 2) return badcommand();
		return run(command_args[1]);
	
	} else return badcommand();
}

int help(){

	char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
	printf("%s\n", help_string);
	return 0;
}

int quit(){
	printf("%s\n", "Bye!");
	exit(0);
}

int set(char* var, char* value){
	mem_set_value(var, value);
	return 0;
}



int echo(char* var) {
    if (var[0] == '$') {
        char *value = mem_get_value(var + 1); 
        if (!strcmp(value, "Variable does not exist")) {
            printf("\n"); 
        } else {
            printf("%s\n", value);
        }
    } else {
        printf("%s\n", var); 
    }
    return 0;
}


int my_mkdir(char *dirname) {
    char* directory = dirname;

    if (dirname[0] == '$') {
        char *value = mem_get_value(dirname + 1); 
        if (value != NULL && strchr(value, ' ') == NULL) {
            directory = value;
        } else {
            printf("%s\n", "Bad command: my_mkdir");
            return -1; 
        }
    }

    int namelen = strlen(directory);
    char command[7 + namelen + 1]; 
    snprintf(command, sizeof(command), "mkdir %s", directory); 

    int errCode = system(command);

    return errCode;
}



int my_cd(char* dirname) {
    struct stat info;
	
    if (stat(dirname, &info) == 0 && S_ISDIR(info.st_mode)) {
		int errCode = chdir(dirname);
        if (errCode == 0) {
            return 0; 
        } 
		else {
            perror("chdir"); 
            return -1; 
        }
    }
    printf("%s\n", "Bad command: my_cd");
	return 2;
}

int print(char* var){
	printf("%s\n", mem_get_value(var)); 
	return 0;
}


int my_ls() {
    const char* command = "ls | sort";
    int errCode = system(command);
    return errCode;
}

int my_touch(char* filename){
	char* file = filename;
	if (filename[0] == '$') {
        char *value = mem_get_value(filename + 1); 
        if (value != NULL && strchr(value, ' ') == NULL) {
            file = value;
        } else {
            printf("%s\n", "Bad command: my_touch");
            return -1; 
        }
    }
	//6 chars for "touch " plus maximum filename length of 100 charcters
	char command[106] = "touch ";
	strcat(command, file);
	int errCode = system(command);
	return errCode;
}

int my_cat(char* filename){
	FILE *file;
	file = fopen(filename, "r");
	if(!file){
		printf("%s\n", "Bad command: my_cat");
		return 0;
	}
	char pointer = fgetc(file);
	while(pointer != EOF){
		printf("%c", pointer);
		pointer = fgetc(file);
	}
	fclose(file);
	return 1;
}

//delete a file in the existing directory
//OPTIONAL QUESTION #9
int my_delete(char* filename){
	if(remove(filename) != 0){
		//prints failure message if file with corresponding filename does not exist in current directory
		printf("%s\n", "Bad command: my_delete");
		return 0;
	}
	return 1;
}


int run(char* script){
	int errCode = 0;
	char line[1000];
	FILE *p = fopen(script,"rt");  // the program is in a file

	if(p == NULL){
		return badcommandFileDoesNotExist();
	}

	fgets(line,999,p);
	while(1){
		errCode = parseInput(line);	// which calls interpreter()
		memset(line, 0, sizeof(line));

		if(feof(p)){
			break;
		}
		fgets(line,999,p);
	}

    fclose(p);

	return errCode;
}
