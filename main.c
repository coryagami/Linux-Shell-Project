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
#define MAX_FILENAME_SIZE 64
#define PATH "PATH"
#define USER "USER"

bool parseInput(arglist* arg_list, char* line);
bool parseBG(arglist* arg_list);
bool parseInRedir(arglist* arg_list, char* redir_file);
bool parseOutRedir(arglist* arg_list, char* redir_file);
bool parseAppendRedir(arglist* arg_list, char* redir_file);
bool parseIoacct(arglist* arg_list);
char* getCmdPath(char* cmd, char* path);
void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes);
void reapChildren();
int parsePipes(arglist* left_args, arglist* right_args);

int main()
{
	char host_name[MAX_SIZE];
	char* user_name = NULL;
	char* curr_dir = NULL;
	char* last_dir = NULL;

	char line[MAX_SIZE];
	char redir_file[MAX_FILENAME_SIZE];
	arglist arg_list, arg_list2, arg_list3;

	int read_bytes = 0;
	int write_bytes = 0;

	// Flags
        bool is_ioacct_cmd;
        bool is_background_process;
        bool is_in_redir;
        bool is_out_redir;
        bool is_append_redir;

	while(1)
	{
		reapChildren();

		// Print prompt
		gethostname(host_name, sizeof(host_name));
		user_name = strdup(getenv(USER));
//		user_name = getlogin();
//		user_name = getpwuid(getuid())->pw_name;
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Parse input
		fgets(line, sizeof(line), stdin);
		
		argListCreate(&arg_list, INIT_ARG_SIZE);
		argListCreate(&arg_list2, INIT_ARG_SIZE);
		argListCreate(&arg_list3, INIT_ARG_SIZE);
		
		if (!parseInput(&arg_list, line))
			continue;
			
		if(parsePipes(&arg_list, &arg_list2)) {};
			parsePipes(&arg_list2, &arg_list3);
			
		argListPrint(&arg_list);
		argListPrint(&arg_list2);
		argListPrint(&arg_list3);			

		is_background_process = parseBG(&arg_list);
		is_in_redir = parseInRedir(&arg_list, redir_file);
		is_out_redir = parseOutRedir(&arg_list, redir_file);
		is_append_redir = parseAppendRedir(&arg_list, redir_file);
		is_ioacct_cmd = parseIoacct(&arg_list);

		if(strcmp(arg_list.args[0], "exit") == 0)
		{
			reapChildren();
			return (arg_list.size == 1) ? 0 : atoi(arg_list.args[1]);
		}

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

			char* cmdPath = getCmdPath(arg_list.args[0], getenv(PATH));

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
					if (is_append_redir)
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
					pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);

					// if(is_ioacct_cmd) executeIoacct(child, &read_bytes, &write_bytes);
				}
				else if (is_ioacct_cmd)
				{
					while(waitpid(-1, (int *)NULL, WNOHANG) == 0)
						executeIoacct(child, &read_bytes, &write_bytes);

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
				if (strcmp(arg_list.args[0], "cd") != 0)
					printf("Command '%s' NOT FOUND!\n", arg_list.args[0]);
			}
		}

		// Memory clean up
		argListDestroy(&arg_list);
		argListDestroy(&arg_list2);
		argListDestroy(&arg_list3);
		free(user_name);
		reapChildren();

	}
	return 0;
}

bool parseInput(arglist* arg_list, char* line)
{
	char* token = strtok(line, " \n");

	while(token)
	{
		argListAdd(arg_list, token, arg_list->size);
		token = strtok(NULL, " \n");
	}

	// Error checking
	if (arg_list->size == 0) return false;
	if (arg_list->size == 1 && strcmp(arg_list->args[0], "ioacct") == 0)
	{
		printf("Usage: ioacct <command>\n");
		return false;
	}

	return true;
}

bool parseBG(arglist* arg_list)
{
        size_t last_arg_len = strlen(arg_list->args[arg_list->size-1]);
	if(arg_list->args[arg_list->size-1][last_arg_len-1] == '&')
	{
		if (last_arg_len > 1)
			arg_list->args[arg_list->size-1][last_arg_len-1] = '\0';
		else if(last_arg_len == 1)
			argListRemove(arg_list, arg_list->size-1);

		return true;
	}
	return false;
}

bool parseInRedir(arglist* arg_list, char* redir_file)
{
	if (arg_list->size >= 3 && strcmp(arg_list->args[arg_list->size-2], "<") == 0)
	{
		strcpy(redir_file, arg_list->args[arg_list->size-1]);
		argListRemove(arg_list, arg_list->size-1);
		argListRemove(arg_list, arg_list->size-1);
		return true;
	}
	return false;
}

bool parseOutRedir(arglist* arg_list, char* redir_file)
{
	if (arg_list->size >= 3 && strcmp(arg_list->args[arg_list->size-2], ">") == 0)
	{
		strcpy(redir_file, arg_list->args[arg_list->size-1]);
		argListRemove(arg_list, arg_list->size-1);
		argListRemove(arg_list, arg_list->size-1);
		return true;
	}
	return false;
}

bool parseAppendRedir(arglist* arg_list, char* redir_file)
{
	if (arg_list->size >= 3 && strcmp(arg_list->args[arg_list->size-2], ">>") == 0)
	{
		strcpy(redir_file, arg_list->args[arg_list->size-1]);
		argListRemove(arg_list, arg_list->size-1);
		argListRemove(arg_list, arg_list->size-1);
		return true;
	}
	return false;
}

bool parseIoacct(arglist* arg_list)
{
	if (strcmp(arg_list->args[0], "ioacct") == 0)
	{
		argListRemove(arg_list, 0);
		return true;
	}
	return false;
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

int parsePipes(arglist* left_args, arglist* right_args)
{
	bool found = false;
	bool another_pipe = false;

	int i=0;
	for(;i < left_args->size; i++)
	{
		if(strcmp(left_args->args[i], "|") == 0)
		{
			found = true;
			break;
		}
	}
	if(!found)
		return;

	argListRemove(left_args, i);
	int numToRemove = left_args->size - i;
	int helper = i;

	for(;i < left_args->size; i++)
	{
		argListAdd(right_args, left_args->args[i], right_args->size);
		if(strcmp(left_args->args[i], "|") == 0)
			another_pipe = true;
	}
	
	int j=0;
	for(;j < numToRemove; j++)
	{
		argListRemove(left_args, helper);
	}
	
	if(another_pipe)
		return 1;
return 0;
}
