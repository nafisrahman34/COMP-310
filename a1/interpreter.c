#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/stat.h> 
#include "shellmemory.h"
#include "shell.h"

int MAX_ARGS_SIZE = 8;

int badcommand(){
	printf("%s\n", "Unknown Command");
	return 1;
}

// For run command only
int badcommandFileDoesNotExist(){
	printf("%s\n", "Bad command: File not found");
	return 3;
}

int help();
int quit();
int set(char* var, char* value);
int echo(char* var);
int print(char* var);
int run(char* script);
int my_cd(char* dirname);
int my_mkdir(char *dirname);
int badcommandFileDoesNotExist();

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
		if (args_size != 2) return badcommand();
		return my_mkdir(command_args[1]);

	} else if (strcmp(command_args[0], "my_cd")==0) {
		if (args_size != 2) return badcommand();
		return my_cd(command_args[1]);
		
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
    if (var == NULL || strlen(var) == 0) {
        printf("Invalid variable name\n");
        return -1;
    }

    if (var[0] == '$') {
        char* value = mem_get_value(var + 1); 
        if (value != NULL) {
            printf("%s\n", value);
        } else {
            printf("Variable %s not found\n", var + 1);
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
        } else {
            perror("chdir"); 
            return -1; 
        }
    }
    printf("%s\n", "Bad command: my_cd");
	return -1;
}

int print(char* var){
	printf("%s\n", mem_get_value(var)); 
	return 0;
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
