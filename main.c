#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "arglist.h"

#define MAX_SIZE 256
#define INIT_ARG_SIZE 5

void parseInput(arglist* arg_list, char* line);

int main() {
	char host_name[MAX_SIZE];
	char* user_name = NULL;
	char* curr_dir = NULL;
//	char* last_dir = NULL;

	char line[MAX_SIZE];
	arglist arg_list;

	for(;;) {
		// Print prompt
		gethostname(host_name, sizeof(host_name));
		user_name = getlogin();
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);


		// Parse input
		fgets(line, sizeof(line), stdin);
		argListCreate(&arg_list, INIT_ARG_SIZE);

		printf("argList Created\n");
		printf("argList Size: %d\n", arg_list.size);
		printf("argList capacity: %d\n", arg_list.capacity);

		parseInput(&arg_list, line);

		printf("argList Size: %d\n", arg_list.size);
		printf("argList capacity: %d\n", arg_list.capacity);

		int i = 0;
		for(;i < arg_list.size; i++)
			printf("arglist element %d is %s\n", i, arg_list.args[i]);

		printf("argList Size: %d\n", arg_list.size);
		printf("argList capacity: %d\n", arg_list.capacity);

		printf("removing element 3\n");
		argListRemove(&arg_list, 3);

		i = 0;
		for(;i < arg_list.size; i++)
			printf("arglist element %d is %s\n", i, arg_list.args[i]);

		printf("argList Size: %d\n", arg_list.size);
		printf("argList capacity: %d\n", arg_list.capacity);


		// Memory clean up
		argListDestroy(&arg_list);
		free(curr_dir);

	}
	return 0;
}

void parseInput(arglist* arg_list, char* line) {
	char* token = strtok(line, " \n");

	while(token && argListAdd(arg_list, token))
		token = strtok(NULL, " ");

	if (arg_list->size > 0) {
		size_t ln = strlen(arg_list->args[arg_list->size-1])-1;
		if (arg_list->args[arg_list->size-1][ln] == '\n')
			arg_list->args[arg_list->size-1][ln] = '\0';
	}
}
