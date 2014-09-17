#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>
#include "arglist.h"
#include "parse.h"

#define MAX_SIZE 256
#define INIT_ARG_SIZE 5
#define MAX_FILENAME_SIZE 64
#define PATH "PATH"
#define USER "USER"

void execBuiltIn(arglist* arg_list, char* curr_dir, char** last_dir, bool is_background_process, bool is_ioacct_cmd);
void execCmd(arglist* arg_list, bool is_background_process, bool is_ioacct_cmd);
pid_t forkAChild(arglist* arg_list, int pipes, int cmd_number, bool is_background_process, bool is_ioacct_cmd, bool is_in_redir, bool is_out_redir, bool is_append_redir, char* redir_file, char* cmd_path, int pipesfd[]);
char* getCmdPath(char* cmd, char* path);
void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes);
void reapChildren();
bool inputErrorHandler(arglist* arg_list);

int main()
{
	char host_name[MAX_SIZE];
	char* user_name = NULL;
	char* curr_dir = NULL;
	char* last_dir = NULL;
	char line[MAX_SIZE];

	arglist* arg_list;

	while(1)
	{
		reapChildren();

		// Print prompt
		gethostname(host_name, sizeof(host_name));
		user_name = strdup(getenv(USER));
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Parse input
		fgets(line, sizeof(line), stdin);


		// Allocate memory for argument lists
		arg_list = malloc(3 * sizeof(arglist));

		argListCreate(&arg_list[0], INIT_ARG_SIZE);
		argListCreate(&arg_list[1], INIT_ARG_SIZE);
		argListCreate(&arg_list[2], INIT_ARG_SIZE);

		if (!parseInput(&arg_list[0], line))
			continue;

		bool is_background_process = parseBG(&arg_list[0]);
		bool is_ioacct_cmd = parseIoacct(&arg_list[0]);

		if(strcmp(arg_list[0].args[0], "exit") == 0 ||
                  (strcmp(arg_list[0].args[0], "cd") == 0 && !is_background_process))
                        execBuiltIn(&arg_list[0], curr_dir, &last_dir, is_background_process, is_ioacct_cmd);
                else
                        execCmd(arg_list, is_background_process, is_ioacct_cmd);

		// Memory clean up
		argListDestroy(&arg_list[0]);
		argListDestroy(&arg_list[1]);
		argListDestroy(&arg_list[2]);
		free(user_name);
		reapChildren();

	}
	return 0;
}

void execBuiltIn(arglist* arg_list, char* curr_dir, char** last_dir, bool is_background_process, bool is_ioacct_cmd)
{
	int read_bytes = 0;
	int write_bytes = 0;

	if(strcmp(arg_list->args[0], "exit") == 0)
	{
		reapChildren();
		if(arg_list->size == 1)
			exit(0);
		else
			exit(atoi(arg_list->args[1]));
	}
	else if(strcmp(arg_list->args[0], "cd") == 0 && !is_background_process)
	{
		if(arg_list->size == 1 || strcmp(arg_list->args[1], "~") == 0)
			chdir(getpwuid(getuid())->pw_dir);
		else if(strcmp(arg_list->args[1], "-") == 0)
			chdir(*last_dir);
		else
			chdir(arg_list->args[1]);

		if(last_dir != NULL) free(*last_dir);
		*last_dir = curr_dir;

		if(is_ioacct_cmd)
		{
			printf("bytes read: -1\n");
			printf("bytes written: -1\n");
		}
	}
}

void execCmd(arglist* arg_list, bool is_background_process, bool is_ioacct_cmd)
{
	char redir_file[MAX_FILENAME_SIZE];

	bool is_in_redir = false;
	bool is_out_redir = false;
	bool is_append_redir = false;

	int read_bytes = 0;
	int write_bytes = 0;

 	size_t pipes = parsePipes(&arg_list[0], &arg_list[1], &arg_list[2]);

	parseRedirs(&arg_list[pipes], &is_in_redir, &is_out_redir, &is_append_redir, redir_file);

	if(strcmp(arg_list[0].args[0], "cd") == 0)
		return;

        char** cmd_path = malloc((pipes + 1) * sizeof(char*));
	int i = 0;
	for(; i < (pipes + 1); i++)
	{
		cmd_path[i] = getCmdPath(arg_list[i].args[0], getenv(PATH));
		if(cmd_path[i] == NULL)
		{
			printf("Command '%s' NOT FOUND!\n", arg_list[i].args[0]);
			return;
		}

		argListRemove(&arg_list[i], 0);
		argListAdd(&arg_list[i], cmd_path[i], 0);
	}

	// Open pipes
	int pipefd[pipes * 2];
	if (pipes > 0) pipe(pipefd);
	if (pipes > 1) pipe(pipefd + 2);

	pid_t child;


	i = 0;
	for(; i < (pipes + 1); i++)
	{
		if (is_ioacct_cmd)
			child = forkAChild(&arg_list[i], pipes, i+1, is_background_process, is_ioacct_cmd, is_in_redir, is_out_redir,
	       		                    is_append_redir, redir_file, cmd_path[i], pipefd);
		else
			forkAChild(&arg_list[i], pipes, i+1, is_background_process, is_ioacct_cmd, is_in_redir, is_out_redir,
	    	                   is_append_redir, redir_file, cmd_path[i], pipefd);
	}

	// Close pipes
	i = 0;
	for(; i < (pipes * 2); i++)
		close(pipefd[i]);

	if (is_ioacct_cmd)
	{
		while(waitpid(-1, (int *)NULL, WNOHANG) == 0)
			executeIoacct(child, &read_bytes, &write_bytes);

		printf("bytes read: %d\n", read_bytes);
		printf("bytes written: %d\n", write_bytes);
	}
	else
	{
		i = 0;
		for(; i < (pipes + 1); i++)
			if(!is_background_process)
				waitpid(-1, (int *)NULL, 0);
			else
				waitpid(-1, (int *)NULL, WNOHANG);
	}
}

pid_t forkAChild(arglist* arg_list, int pipes, int cmd_number, bool is_background_process, bool is_ioacct_cmd, bool is_in_redir, bool is_out_redir, 
                bool is_append_redir, char* redir_file, char* cmd_path, int pipefd[])
{
	pid_t child = fork();
	if (child == 0)
	{
		if(cmd_number == 1 && pipes > 0)
		{
//			if(pipes == 1) dup2(pipefd[0], 0);
			dup2(pipefd[1], 1);
		}
		else if(cmd_number == pipes + 1 || pipes == 0)
		{

//			if(pipes == 1) dup2(pipefd[1], 1);
			if(pipes == 1) dup2(pipefd[0], 0);
			if(pipes == 2) dup2(pipefd[2], 0);

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
			if (is_append_redir)
			{
				int fd = open(redir_file, O_WRONLY|O_APPEND);
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}
		}
		else
		{
			dup2(pipefd[0], 0);
			dup2(pipefd[3], 1);
		}

		if(pipes > 0)
		{
			int i = 0;
			for(; i < (pipes * 2); i++)
				close(pipefd[i]);
		}

		execv(cmd_path, arg_list->args);
	}
	else
		return child;
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
	char filename[MAX_FILENAME_SIZE];
	sprintf(filename, "/proc/%d/io", (int)pid);

	FILE* fp = fopen(filename, "r");
	if(fp == NULL)
	{
	}
        else
	{
                char line[MAX_SIZE];
                int i = 0;
                for(;i < 2; i++)
		{
                        fgets(line, sizeof(line), fp);
                        if(i == 0 || i == 1)
			{
				char* byte_data = strtok(line, " \n");
				byte_data = strtok(NULL, " \n");

				if (byte_data != NULL)
				{
					if(i == 0)
						*read_bytes = atoi(byte_data);
					else if(i == 1)
						*write_bytes = atoi(byte_data);
				}
			}
                }
		fclose(fp);
        }
}

void reapChildren()
{
	while(waitpid(-1, (int *)NULL, WNOHANG) > 0) {};
}
