#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>

size_t parseInput(char* cmd, char** args, size_t* args_size, char* line);
char* cmdExist(char* cmd, char* path);

int main()
{
	system("clear");		/*	might not be allowed */

	char host_name[64];
	char* user_name = malloc(64);
	char curr_dir[1024];

	size_t args_size;
	char** args;
	char* cmd;
	size_t num_args;
	char line[256];

	/* maybe put below 2 functions inside loop? */
	gethostname(host_name, sizeof(host_name));
	user_name = getlogin();

	while(1)
	{
		// Print prompt
		getcwd(curr_dir, sizeof(curr_dir));
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Allocate memory for commandline
		fgets(line, sizeof(line), stdin);

		args_size = 4;
		args = malloc(args_size * sizeof(char*));

		num_args = parseInput(cmd, args, &args_size, line);

		if(num_args == 0)
			continue;

		printf("%p\n", args);
		printf("%d", num_args);
		int i = 0;
		while (i <= num_args-1)
			printf("\n%p\n", args[i++]);

		if(strcmp(cmd, "cd") == 0 || strcmp(cmd, "ioacct") == 0 || strcmp(cmd, "exit") == 0) {
			if (strcmp(cmd, "cd") == 0) {
				chdir(args[1]);
			}
			else if (strcmp(cmd, "ioacct") == 0) {
				// do ioacct
			}
			else if (strcmp(cmd, "exit") == 0) {
				if(args[1] == NULL)
					return 0;
				else
					return atoi(args[1]);
			}
		}
		else {
			// Searching for command here

			char* cmdPath = cmdExist(cmd, getenv("PATH"));

			// IF FOUND ------------------------------------------------
			if(cmdPath != NULL) {
				// printf("Command '%s' FOUND in %s !\n", args[0], cmdPath);
				strcpy(args[0], cmdPath);

				pid_t child = fork();
				if (child == 0) {
					int rtn = execv(cmdPath, (char **)args);
				}
				else if (child < 0) {
					// fork failed
					exit(EXIT_FAILURE);
				}
				// else if & run background
				else {
					pid_t child_finished;
					child_finished = waitpid(-1, (int *)NULL, 0);
				}
			}

			// IF NOT FOUND --------------------------------------------
			else {
				printf("Command '%s' NOT FOUND!\n", args[0]);
			}
		}
		int x = 0;
		for(; x <= num_args-1; x++)
			free (args[x]);
		free (args);
	}
}

size_t parseInput(char* cmd, char** args, size_t* args_size, char* line)
{
	char* token = strtok(line, " \n");

	size_t num_args = 0;
	while(token) {
		if (num_args >= *args_size) {
			*args_size *= 2;
			args = realloc(args, *args_size * sizeof(char*));
		}

		args[num_args++] = strdup(token);
		token = strtok(NULL, " ");
	}

	if (num_args > 0) {
		size_t ln = strlen(args[num_args-1])-1;
		if(args[num_args-1][ln] == '\n')
			args[num_args-1][ln] = '\0';

		strcpy(cmd, args[0]);
	}

//	if (num_args >= args_size)
//		resize(args, args_size);
//	args[num_args++] = (char*)NULL;

	int i = 0;
	while (i <= num_args-1)
		printf("\n%p\n", args[i++]);

	printf("%p\n", args);
	return num_args;
}

char* cmdExist(char* cmd, char* path)
{
	DIR *d;
	struct dirent *dir;
	char* token;
	char* cmdPath;

	// first path, strtok on 'path'
	token = strtok(strdup(path), ":");
	d = opendir(strdup(token));
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
			if(strcmp(cmd, dir->d_name) == 0) {
				cmdPath = token;
				strcat(cmdPath, "/");
				closedir(d);
				return strcat(cmdPath, cmd);
			}
		closedir(d); // ADD MORE THESE
	}

	// rest of paths, strtok on 'NULL'
	while(token = strtok(NULL, ":"))
	{
		d = opendir(strdup(token));
		if(d)
		{
			while ((dir = readdir(d)) != NULL)
				if(strcmp(cmd, dir->d_name) == 0) {
					cmdPath = token;
					strcat(cmdPath, "/");
					closedir(d);
					return strcat(cmdPath, cmd);
				}
		}
	}
	closedir(d);
	return NULL;
}
