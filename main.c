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
#include <ctype.h>
#include "arglist.h"


#define MAX_SIZE 256
#define INIT_ARG_SIZE 5
#define MAX_REDIR_FILE_SIZE 64


void parseInput(arglist* arg_list, char* line, bool* is_in_redir, bool* is_out_redir, bool* is_append, char* redir_file);
char* getCmdPath(char* cmd, char* path);
void executeIoacct(pid_t pid);

int main()
{
	char host_name[MAX_SIZE];
	char* user_name = NULL;
	char* curr_dir = NULL;
	char* last_dir = NULL;

	char line[MAX_SIZE];
	char redir_file[MAX_REDIR_FILE_SIZE];
	arglist arg_list;

	char* read_bytes = NULL;
	char* write_bytes = NULL;

	// Flags
        bool is_ioacct_cmd;
        bool is_background_process;
        bool is_in_redir;
        bool is_out_redir;
        bool is_append;

	for(;;)
	{
                // Reset flags
                is_ioacct_cmd = false;
                is_background_process = false;
                is_in_redir = false;
                is_out_redir = false;
                is_append = false;

		// Print prompt
		gethostname(host_name, sizeof(host_name));
		user_name = getlogin();
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Parse input
		fgets(line, sizeof(line), stdin);
		argListCreate(&arg_list, INIT_ARG_SIZE);
		parseInput(&arg_list, line, &is_in_redir, &is_out_redir, &is_append, redir_file);

		if (arg_list.size == 0) continue;

		if (strcmp(arg_list.args[0], "ioacct") == 0)
		{
			argListRemove(&arg_list, 0);
			is_ioacct_cmd = true;
		}

                // Check if the command should be a background process
                size_t last_arg_len = strlen(arg_list.args[arg_list.size-1]);
                if(arg_list.args[arg_list.size-1][last_arg_len-1] == '&')
		{
                        if (last_arg_len > 1)
                                arg_list.args[arg_list.size-1][last_arg_len-1] = '\0';
                        else if(last_arg_len == 1)
				argListRemove(&arg_list, arg_list.size-1);

                        is_background_process = true;
                }

		if(strcmp(arg_list.args[0], "exit") == 0)
			return (arg_list.size == 1) ? 0 : atoi(arg_list.args[1]);

		if(strcmp(arg_list.args[0], "cd") == 0 && !is_background_process)
		{
			if(arg_list.size == 1 || strcmp(arg_list.args[1], "~") == 0)
				chdir(getpwuid(getuid())->pw_dir);
			else if(strcmp(arg_list.args[1], "-") == 0)
				chdir(last_dir);
			else
				chdir(arg_list.args[1]);

			if(last_dir != NULL) free(last_dir);
			last_dir = curr_dir;

			if(is_ioacct_cmd) executeIoacct(getpid());

		}
		else
		{
			char* cmdPath = getCmdPath(arg_list.args[0], getenv("PATH"));


			if (cmdPath != NULL)
			{
				argListRemove(&arg_list, 0);
				argListAdd(&arg_list, cmdPath, 0);
                                pid_t child = fork();

				if (child == 0)
				{
					fflush(0);

					// Input redirect
					if (is_in_redir)
					{
						int fd = open(redir_file, O_RDONLY);
						dup2(fd, STDIN_FILENO);
						close(fd);
					}

					// Output redirect
					if (is_out_redir)
					{
						int fd = creat(redir_file, 0644);
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}

					// Append redirect
					if (is_append)
					{
						int fd = open(redir_file, O_WRONLY|O_APPEND);
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}

					int rtn = execv(cmdPath, arg_list.args);
				}
				else if (child < 0)
				{
					exit(EXIT_FAILURE);
				}
				else if (is_background_process)
				{
//					printf("back!\n");
//					pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);

					int status;
					pid_t pid;
					while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
						printf("[proc %d exited with code %d]\n",
						pid, WEXITSTATUS(status));
					}

					if(is_ioacct_cmd) executeIoacct(child);
				}
				else
				{
//					pid_t child_finished = waitpid(-1, (int *)NULL, 0);
					int status;
					pid_t pid;
					pid = waitpid(-1, &status, 0);
					if (WIFEXITED(status)) {
						printf("[proc %d exited with code %d]\n", pid, WEXITSTATUS(status));
					}

					if(is_ioacct_cmd) executeIoacct(child);
				}
			}
			else
			{
				printf("Command '%s' NOT FOUND!\n", arg_list.args[0]);
			}
		}

		// Memory clean up
		argListDestroy(&arg_list);

	}
	return 0;
}

void parseInput(arglist* arg_list, char* line, bool* is_in_redir, bool* is_out_redir, bool* is_append, char* redir_file)
{
	char* token = strtok(line, " \n");

	while(token)
	{
		argListAdd(arg_list, token, arg_list->size);
		token = strtok(NULL, " \n");
	}

       if (arg_list->size >= 3) {
                if (strcmp(arg_list->args[arg_list->size-2], "<") == 0) *is_in_redir = true;
                if (strcmp(arg_list->args[arg_list->size-2], ">") == 0) *is_out_redir = true;
                if (strcmp(arg_list->args[arg_list->size-2], ">>") == 0) *is_append = true;

                if (*is_out_redir || *is_in_redir || *is_append) {
			strcpy(redir_file, arg_list->args[arg_list->size-1]);
			argListRemove(arg_list, arg_list->size-1);
			argListRemove(arg_list, arg_list->size-1);
		}
       }
}

char* getCmdPath(char* cmd, char* path)
{
	DIR *d;
	struct dirent *dir;
	char* token;
	char* cmdPath;
	path = strdup(path);

	token = strtok(path, ":");
        while(token)
        {
                d = opendir(token);
                if(d)
		{
                        while ((dir = readdir(d)) != NULL)
                                if(strcmp(cmd, dir->d_name) == 0)
				{
                                        cmdPath = token;
                                        strcat(cmdPath, "/");
                                        strcat(cmdPath, cmd);
					closedir(d);
					return cmdPath;
                                }
                }
		closedir(d);
		token = strtok(NULL, ":");
	}
	free(path);
	return NULL;
}

void executeIoacct(pid_t pid)
{
	char pid_c[10];
	char filename[] = "/proc/";
	snprintf(pid_c, 10, "%d", (int)pid);
	strcat(filename, pid_c);
	strcat(filename, "/io");

	printf("%s\n", filename);

	FILE* fp;
	fp = fopen(filename, "r");
	if(fp == NULL)
	{
		printf("read bytes: -1\n");
		printf("write bytes: -1\n");
	}
        else
	{
                char line[64];
                int i = 0;
                for(;i < 6; i++) {
                        fgets(line, sizeof(line), fp);
                        if(i == 4 || i == 5)
			{
				char* byte_data = strtok(line, " ");
				byte_data = strtok(NULL, " \n");

				if(i == 4)
					printf("read bytes: %s\n", byte_data);
				else if(i == 5)
					printf("write bytes: %s\n", byte_data);
			}
                }
        }
}
