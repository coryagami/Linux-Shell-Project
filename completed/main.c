/*********************************************************************************
		 ____  ____   _   ____  ____ ____
		|  _ \|  __| / \ |  _ \|  __|  _ \
		| |_)|| |_  / _ \| |_) | |_ | |_)|
		|   < |  _|( |_| )  __/|  _||   <
		| |\ \| |__|  _  | |   | |__| |\ \
		|_| \_\____|_| |_|_|   |____|_| \_\


  Reaper is a basic shell that supports command execution, I/O output, input
  and append redirection, background processing, and piping. Commands supported
  include any executable file located in the PATH environment variables, as well
  as the built in commands cd, and exit. Included in cd's operation is support
  for the - and ~ operands as well as no operand. Input and output redirects
  cannot be combined in one command. However they can be used in combination
  with piping such that the commands is not ambiguous. Piping support is limited
  to two pipes. Background processingis supported by appending an & as the last
  non-white space character to a command. Additionally support for an ioacct
  command of the form ioacct <command>is supported such that the command
  following ioacct is executed on the as if typed directly into the shell.
  Ioacct prints out the character bytes read and  written by the process
  executing that command. Ioacct combinations with background processing, piping,
  and I/O output, input, and append redirections is notsupported.

  Developers: Cory Agami & Javier Lores

*********************************************************************************/

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

// **********************************************************
// **********************************************************
// *                     CONSTANTS                          *
// **********************************************************
// **********************************************************

#define MAX_SIZE 256
#define INIT_ARG_SIZE 5
#define MAX_FILENAME_SIZE 64
#define PATH "PATH"
#define USER "USER"

// **********************************************************
// **********************************************************
// *                     FUNCTIONS                          *
// **********************************************************
// **********************************************************

void execBuiltIn(arglist* arg_list, char* curr_dir, char** last_dir, bool is_background_process, bool is_ioacct_cmd);
void execCmd(arglist* arg_list, bool is_background_process, bool is_ioacct_cmd, char* curr_dir);
pid_t forkAChild(arglist* arg_list, int pipes, int cmd_number, bool is_background_process, bool is_ioacct_cmd, bool is_in_redir, 
                 bool is_out_redir, bool is_append_redir, char* redir_file, char* cmd_path, int pipesfd[]);
char* getCmdPath(char* cmd, char* path);
void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes);
void reapChildren();
bool inputErrorHandler(arglist* arg_list);

// **********************************************************
// **********************************************************
// *  Main Routine                                          *
// *	- Declare and initalize variables		    *
// *	- Enter infinite loop that exits when the user      *
// *      types the built-in exit command.                  *
// *        - Print the prompt                              *
// *        - Allocate memory for argument lists            *
// *        - Parse user input                              *
// *        - Check for if background process or ioacct     *
// *          command was entered.                          *
// *        - Run the command entered                       *
// *        - Clean up memory                               *
// *    - Rinse and repeat                                  *
// **********************************************************
// **********************************************************

int main()
{
	// Declare and initialize variables
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

		// Allocate memory for argument lists
		arg_list = malloc(3 * sizeof(arglist));
		argListCreate(&arg_list[0], INIT_ARG_SIZE);
		argListCreate(&arg_list[1], INIT_ARG_SIZE);
		argListCreate(&arg_list[2], INIT_ARG_SIZE);

		// Parse input
		fgets(line, sizeof(line), stdin);
		if (!parseInput(&arg_list[0], line))
			continue;

		// Check for background processes and ioacct command
		bool is_background_process = parseBG(&arg_list[0]);
		bool is_ioacct_cmd = parseIoacct(&arg_list[0]);

		// If its a builtin run execBuiltIn, other run execCmd
		if(strcmp(arg_list[0].args[0], "exit") == 0 ||
                  (strcmp(arg_list[0].args[0], "cd") == 0 && !is_background_process))
                        execBuiltIn(&arg_list[0], curr_dir, &last_dir, is_background_process, is_ioacct_cmd);
                else
                        execCmd(arg_list, is_background_process, is_ioacct_cmd, curr_dir);

		// Memory and process clean up
		argListDestroy(&arg_list[0]);
		argListDestroy(&arg_list[1]);
		argListDestroy(&arg_list[2]);
		free(arg_list);
		free(user_name);
		reapChildren();

	}
	return 0;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: execBuiltIn                                                 *
// * Description: This function executes the built in commands of the shell     *
// * Parameters:                                                                *
// *    arglist* arglist           : The argument list of the commands.         *
// *    char* curr_dir             : The current directoy                       *
// *    char** last_dir            : The last directory                         *
// *    bool is_background_Process : Whether or not the command should be       *
// *                                 executed in the background                 *
// *    bool is_ioacct_cmd         : Whether or not the command is an ioacct    *
// *                                                                            *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void execBuiltIn(arglist* arg_list, char* curr_dir, char** last_dir, bool is_background_process, bool is_ioacct_cmd)
{
	int read_bytes = 0;
	int write_bytes = 0;

	// Execute exit
	if(strcmp(arg_list->args[0], "exit") == 0)
	{
		reapChildren();
		if(arg_list->size == 1)
			exit(0);
		else
			exit(atoi(arg_list->args[1]));
	}
	// Execute "cd"
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

// ******************************************************************************
// ******************************************************************************
// * Function Name: execCmd                                                     *
// * Description: This function executes all other commands                     *
// * Parameters:                                                                *
// *    arglist* arglist           : The argument list of the commands.         *
// *    bool is_background_Process : Whether or not the command should be       *
// *                                 executed in the background                 *
// *    bool is_ioacct_cmd         : Whether or not the command is an ioacct    *
// *                                                                            *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void execCmd(arglist* arg_list, bool is_background_process, bool is_ioacct_cmd, char* curr_dir)
{
	// Declare and initialize variables
	char redir_file[MAX_FILENAME_SIZE];

	bool is_in_redir = false;
	bool is_out_redir = false;
	bool is_append_redir = false;

	int read_bytes = 0;
	int write_bytes = 0;

	// Parse pipes and redirects
 	size_t pipes = parsePipes(&arg_list[0], &arg_list[1], &arg_list[2]);
	parseRedirs(&arg_list[pipes], &is_in_redir, &is_out_redir, &is_append_redir, redir_file);

	if(strcmp(arg_list[0].args[0], "cd") == 0)
		return;

	char** cmd_path = malloc((pipes + 1) * sizeof(char*));
	if(arg_list[0].args[0][0] == '.' && arg_list[0].args[0][1] == '/')
	{
		char* substring = malloc(strlen(arg_list[0].args[0]));
		strncpy(substring, arg_list[0].args[0]+2, strlen(arg_list[0].args[0]-1));
		argListRemove(&arg_list[0], 0);
		argListAdd(&arg_list[0], substring, 0);

		cmd_path[0] = malloc(strlen(substring) + strlen(curr_dir)  + 1);
		strcpy(cmd_path[0], curr_dir);
		strcat(cmd_path[0], "/");
		strcat(cmd_path[0], substring);
		free(substring);
	}
	else
	{
		// Find the paths in the env PATH variable to the commands and add them to the arguments
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
	}

	// If there are any pipes setup the file descriptors and open them
	int pipefd[pipes * 2];
	if (pipes > 0) pipe(pipefd);
	if (pipes > 1) pipe(pipefd + 2);

	pid_t child;

	reapChildren();

	// Fork childs as necessary
	int i = 0;
	for(; i < (pipes + 1); i++)
	{
		if (is_ioacct_cmd)
			child = forkAChild(&arg_list[i], pipes, i+1, is_background_process, is_ioacct_cmd, is_in_redir, is_out_redir,
	       		                    is_append_redir, redir_file, cmd_path[i], pipefd);
		else
			forkAChild(&arg_list[i], pipes, i+1, is_background_process, is_ioacct_cmd, is_in_redir, is_out_redir,
	    	                   is_append_redir, redir_file, cmd_path[i], pipefd);
	}

	// Close pipes as necessary
	i = 0;
	for(; i < (pipes * 2); i++)
		close(pipefd[i]);

	// Handle the ioacct command output
	if (is_ioacct_cmd)
	{
		while(waitpid(-1, (int *)NULL, WNOHANG) == 0)
			executeIoacct(child, &read_bytes, &write_bytes);

		printf("bytes read: %d\n", read_bytes);
		printf("bytes written: %d\n", write_bytes);
	}
	// Handle background processes and clean up children as necessary
	else
	{
		i = 0;
		for(; i < (pipes + 1); i++)
			if(!is_background_process)
				waitpid(-1, (int *)NULL, 0);
			else
				waitpid(-1, (int *)NULL, WNOHANG);
	}

	// Memory cleanup
	free(cmd_path);
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: execCmd                                                     *
// * Description: This function executes all other commands                     *
// * Parameters:                                                                *
// *    arglist* arglist           : The argument list of the commands.         *
// *    bool is_background_Process : Whether or not the command should be       *
// *                                 executed in the background                 *
// *    bool is_ioacct_cmd         : Whether or not the command is an ioacct    *
// *                                                                            *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

pid_t forkAChild(arglist* arg_list, int pipes, int cmd_number, bool is_background_process, bool is_ioacct_cmd, bool is_in_redir, bool is_out_redir, 
                bool is_append_redir, char* redir_file, char* cmd_path, int pipefd[])
{
	pid_t child = fork();
	if (child == 0)
	{
		// The first command of the pipe
		if(cmd_number == 1 && pipes > 0)
		{
			dup2(pipefd[1], 1);
		}
		// The last command of the pipe
		else if(cmd_number == pipes + 1 || pipes == 0)
		{

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
		// Any intermediary pipes
		else
		{
			dup2(pipefd[0], 0);
			dup2(pipefd[3], 1);
		}

		// Close pipes as necessary
		int i = 0;
		for(; i < (pipes * 2); i++)
			close(pipefd[i]);

		// Execute the command
		execv(cmd_path, arg_list->args);
	}
	else
		return child;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: getCmdPath                                                  *
// * Description: This function finds and returns the path in the env PATH      *
// * variable to the command passed in                                          *
// * Parameters:                                                                *
// *    char* cmd  : The command to search for.                                 *
// *    char* path : The path to search in.                                     *
// *                                                                            *
// * Return: char*                                                              *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************


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

// ******************************************************************************
// ******************************************************************************
// * Function Name: executeIoacct                                               *
// * Description: This function continuously reads the io file of a process     *
// * until it closes and the return through its paramters the number of bytes   *
// * read and written by that process.                                          *
// * Parameters:                                                                *
// *    pid_t pid        : The process id of the process whose io information   *
// *                       will be read                                         *
// *    int* read_bytes  : The location store the number of bytes read          *
// *    int* write_bytes : The location to store the number of bytes written    *
// *                                                                            *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void executeIoacct(pid_t pid, int* read_bytes, int* write_bytes)
{
	char filename[MAX_FILENAME_SIZE];
	sprintf(filename, "/proc/%d/io", (int)pid);

	// Open the file
	FILE* fp = fopen(filename, "r");
	if(fp == NULL)
	{
	}
        else
	{
                char line[MAX_SIZE];
                int i = 0;
		// Read the first two lines
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

// ******************************************************************************
// ******************************************************************************
// * Function Name: reapChildren                                                *
// * Description: This function reaps any terminated child processes.           *
// * Parameters: None                                                           *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void reapChildren()
{
	while(waitpid(-1, (int *)NULL, WNOHANG) > 0) {};
}
