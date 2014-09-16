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
void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes);
void parsePipes(arglist* args_list1, arglist* arg_list2);

int main()
{
	char host_name[MAX_SIZE];
	char* user_name = NULL;
	char* curr_dir = NULL;
	char* last_dir = NULL;

	char line[MAX_SIZE];
	char redir_file[MAX_REDIR_FILE_SIZE];
	arglist arg_list, arg_list2;

	int read_bytes = 0;
	int write_bytes = 0;

	// Flags
        bool is_ioacct_cmd;
        bool is_background_process;
        bool is_in_redir;
        bool is_out_redir;
        bool is_append;

	while(1)
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
//		user_name = getpwuid(getuid())->pw_name;
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Parse input
		fgets(line, sizeof(line), stdin);
		argListCreate(&arg_list, INIT_ARG_SIZE);
		argListCreate(&arg_list2, INIT_ARG_SIZE);
		parseInput(&arg_list, line, &is_in_redir, &is_out_redir, &is_append, redir_file);

		parsePipes(&arg_list, &arg_list);


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

			if(is_ioacct_cmd)
			{
				printf("bytes read: -1\n");
				printf("bytes written: -1\n");
			}

		}
		else
		{
			char* cmdPath = getCmdPath(arg_list.args[0], getenv("PATH"));


			if (cmdPath != NULL)
			{
				argListRemove(&arg_list, 0);
				argListAdd(&arg_list, cmdPath, 0);

				// To catch/cleanup old background processes
				pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);
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
					pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);

//					if(is_ioacct_cmd) executeIoacct(child, &read_bytes, &write_bytes);
				}
				else if (is_ioacct_cmd)
				{
					while(waitpid(-1, (int *)NULL, WNOHANG) == 0)
					{
						executeIoacct(child, &read_bytes, &write_bytes);
					}
					printf("bytes read: %d\n", read_bytes);
					printf("bytes written: %d\n", write_bytes);
				}
				else
				{
					pid_t child_finished = waitpid(-1, (int *)NULL, 0);
				}
			}
			else
			{
				printf("Command '%s' NOT FOUND!\n", arg_list.args[0]);
			}
		}

		// Memory clean up
		argListDestroy(&arg_list);
		argListDestroy(&arg_list2);
//		free(user_name);

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

void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes)
{
	char filename[64];
	sprintf(filename, "/proc/%d/io", (int)pid);

//	printf("%s\n", filename);

	FILE* fp;
	fp = fopen(filename, "r");
	if(fp == NULL)
	{
//		printf("read bytes: -1\n");
//		printf("write bytes: -1\n");
	;}
        else
	{
                char line[64];
                int i = 0;
                for(;i < 2; i++)
		{
                        fgets(line, sizeof(line), fp);
                        if(i == 0 || i == 1)
			{
				char* byte_data = strtok(line, " \n");
				byte_data = strtok(NULL, " \n");

				if(i == 0)
					*read_bytes = atoi(byte_data);
				else if(i == 1)
					*write_bytes = atoi(byte_data);
			}
                }
        }
}

void parsePipes(arglist* args_list1, arglist* args_list2)
{
	argListPrint(args_list1);
	bool found = false;

	int i=0;
	for(;i < args_list1->size; i++)
	{
		if(strcmp(args_list1->args[i], "|") == 0)	// check for spaces or whatever
		{
			found = true;
			break;
		}
	}
	if(!found)
		return;

	printf(":%d:", i);

	argListRemove(args_list1, i);
	int numToRemove = args_list1->size - i;

	for(;i <= args_list1->size; i++)
	{
		argListAdd(args_list2, args_list1->args[i], args_list2->size);
	}
	int j=0;
	for(;j < numToRemove-1; j++)
	{
		argListRemove(args_list1, 2);
	}
	argListPrint(args_list1);
	argListPrint(args_list2);

}
