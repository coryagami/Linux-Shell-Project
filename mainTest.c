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
#include <signal.h>

#define MAX_MEM_SIZE 256
#define MAX_REDIR_FILESIZE 64

int parseInput(char* args[], char* line, bool* is_in_redir, bool* is_out_redir, bool* is_append, char* redir_file);
char* cmdExist(char* cmd, char* path);
int doIoacct(pid_t pid, char* read_bytes, char* write_bytes, bool* read_io);
void block_sigtou_signal();

int main()
{
	system("clear");		/*	might not be allowed */

	char host_name[MAX_MEM_SIZE];
	char* user_name;		// check if we can use this with beter login function, maybe needs allocation first
	char* curr_dir;
	char* last_dir = NULL;			// maybe set all pointers to NULL at initialization

	char* args0;
	char* args1;
	char* args2;
	char* args3;
	char* args[] = {args0, args1, args2, args3, (char *)NULL};			// FIGURE THIS OUT!
	char line[MAX_MEM_SIZE];
	char redir_file[MAX_REDIR_FILESIZE];


	// Flags
	bool is_ioacct_cmd;
	bool is_background_process;
	bool is_in_redir;
	bool is_out_redir;
	bool is_append;
	bool is_one_pipe;
	bool is_two_pipe;
	char* read_bytes;
	char* write_bytes;
	bool read_io;

	while(1)
	{
		// Reset flags
		is_ioacct_cmd = false;
		is_background_process = false;
		is_in_redir = false;
		is_out_redir = false;
		is_append = false;
		is_one_pipe = false;
		is_two_pipe = false;
		// read_bytes = "-99";	// doesnt let me do this...
		// write_bytes = "-99";
		read_io = false;


		// Reset arguments
		args[0] = args[1] = args[2] = args[3] = NULL;

		// Print prompt
		gethostname(host_name, sizeof(host_name));
		// user_name = getpwuid(getuid())->pw_name;			// non deprecated way, but seg fault with ioacct
		user_name = getlogin();
		curr_dir = getcwd(NULL, 0);
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		// Parse input
		fgets(line, sizeof(line), stdin);
		int num_args = parseInput(args, line, &is_out_redir, &is_in_redir, &is_append, redir_file);

		if(num_args == 0)
			continue;
	/*
		if (is_in_redir) printf("In redir ");
		if (is_out_redir) printf("out redir ");
		if (is_append) printf("append ");
		if (is_out_redir || is_in_redir || is_append) printf("file: %s", redir_file); */

		// Run command
		if(strcmp(args[0], "ioacct") == 0) {
			int i = 0;
			for(; i < num_args; i++) {
				args[i] = args[i+1];
			}
			is_ioacct_cmd = true;
			num_args--;
		}

		// Check if the command should be a background process
		size_t last_arg_len = strlen(args[num_args-1]);
		if(args[num_args-1][last_arg_len-1] == '&') {
			if (last_arg_len > 1)
				args[num_args-1][last_arg_len-1] = '\0';
			else if(last_arg_len == 1) {
				free(args[num_args-1]);
				args[--num_args] = NULL;
			}
			is_background_process = true;
		}
		
		// printf("\n%s %s %s %s\n", args[0], args[1], args[2], args[3]);

		if(strcmp(args[0], "cd") == 0 || strcmp(args[0], "exit") == 0) {
			if (strcmp(args[0], "cd") == 0) {
				if(args[1] == NULL || strcmp(args[1], "~") == 0) {
					struct passwd* pw = getpwuid(getuid());
					chdir(pw->pw_dir);
				}
				else if(strcmp(args[1], "-") == 0)
					chdir(last_dir);
				else
					chdir(args[1]);
				if(last_dir != NULL)
					free(last_dir);
				last_dir = strdup(curr_dir);
			}
			else if (strcmp(args[0], "exit") == 0) {
				return (args[1] == NULL) ? 0 : atoi(args[1]);
			}
			if(is_ioacct_cmd) {
					doIoacct(getpid(), read_bytes, write_bytes, &read_io);
					
					if(!read_io) {
							printf("read bytes: -1\n");
							printf("write bytes: -1\n");
						}
						else {
							printf("read bytes: %s\n", read_bytes);
							printf("write bytes: %s\n", write_bytes);
						}
			}
		}
		else {
			// Searching for command here

			char* cmdPath = cmdExist(args[0], getenv("PATH"));

			// IF FOUND ------------------------------------------------
			if(cmdPath != NULL) {
				// printf("Command '%s' FOUND in %s !\n", args[0], cmdPath);
				strcpy(args[0], cmdPath);
				pid_t child = fork();
				
				if (child == 0) {
					fflush(0);		// dont know what this does...
					
					// Input redirect
					if (is_in_redir) {
						int fd = open(redir_file, O_RDONLY);
						dup2(fd, STDIN_FILENO);
						close(fd);
					}
					
					// Output redirect
					if (is_out_redir) {
						int fd = creat(redir_file, 0644);
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}
					
					// Append redirect
					if (is_append) {
						int fd = open(redir_file, O_WRONLY|O_APPEND);
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}

					int rtn = execv(cmdPath, (char **)args);
				}
				else if (child < 0) {
					// fork failed
					exit(EXIT_FAILURE);
				}
				else if (is_background_process) {
					block_sigtou_signal();
					
					printf("back!");
					pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);
				}
				else {
					block_sigtou_signal();
					
					if(is_ioacct_cmd) {
						pid_t child_finished = waitpid(-1, (int *)NULL, WNOHANG);
						while(true) {
							sleep(.05);
							if(doIoacct(child, read_bytes, write_bytes, &read_io) == 0) {
								break;
							}
						}
						if(!read_io) {
							printf("read bytes: -1\n");
							printf("write bytes: -1\n");
						}
						else {
							printf("read bytes: %s\n", read_bytes);
							printf("write bytes: %s\n", write_bytes);
						}
					}
					else {
						pid_t child_finished = waitpid(-1, (int *)NULL, 0);
					}
				}
			}

			// IF NOT FOUND --------------------------------------------
			else {
				printf("Command '%s' NOT FOUND!\n", args[0]);
			}
		}

		// freeing args memory accordingly
		int x = 0;
		for(; x < num_args; x++) {		// FIX THIS when 'man' then 'q' it fails
			// printf("1");
			free(args[x]);
			// printf("2");
		}
		free(curr_dir);
		// free(user_name);
		// free(pass);
	}
	free(curr_dir);
	free(last_dir);
}

int parseInput(char* args[], char* line, bool* is_out_redir, bool* is_in_redir, bool* is_append, char* redir_file)
{
	char* token = strtok(line, " \n");

	int num_args = 0;
	while(token) {
		
		/*									*** piping code, waiting for xavier implementation ***
		if(token == '|' && !*is_one_pipe) {
			*is_one_pipe = true;
			
			continue;
		}
		else if(token == '|' && *is_one_pipe) {
			
			continue;
		}
		*/
		
		args[num_args++] = strdup(token);

		token = strtok(NULL, " ");
	}

	if (num_args > 0) {
		size_t ln = strlen(args[num_args-1])-1;
		if(args[num_args-1][ln] == '\n')
			args[num_args-1][ln] = '\0';
	}

	if (num_args >= 3) {
		if (strcmp(args[num_args-2], "<") == 0) *is_in_redir = true;
		if (strcmp(args[num_args-2], ">") == 0) *is_out_redir = true;
		if (strcmp(args[num_args-2], ">>") == 0) *is_append = true;

		if (*is_out_redir || *is_in_redir || *is_append) {
			strcpy(redir_file, args[num_args-1]);

			// Memory clean up
			free(args[num_args-2]);
			free(args[num_args-1]);
			args[num_args-2] = NULL;
			args[num_args-1] = NULL;
			num_args -= 2;
			
		
		}
	}

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

int doIoacct(pid_t pid, char* read_bytes, char* write_bytes, bool* read_io)
{
	char pid_c[10];
	char filename[] = "/proc/";
	snprintf(pid_c, 10, "%d", (int)pid);
	strcat(filename, pid_c);
	strcat(filename, "/io");

	//printf("%s\n", filename);

	FILE* fp;
	fp = fopen(filename, "r");
	if(fp == NULL) {
		// printf("Cant open io file at %s, errno: %d\n", filename, errno);
		fflush(0);		// need this?
		
		if(!*read_io)
			return -99;	// return this if no chane to read io
			
		return 0;
	}
	else {
		char line[64];
		int i=0;
		for(;i<6; i++) {
			fgets(line, sizeof(line), fp);
			if(i == 4 || i == 5) {
				
				// parsing out read/write byte data
				char* byte_data = strtok(line, " ");
				byte_data = strtok(NULL, " \n");
				if(i==4)
					strcpy(read_bytes, byte_data);
				else if(i==5)
					strcpy(write_bytes, byte_data);
			}
			*read_io = true;
		}
		return 1;
	}
}

void block_sigtou_signal() {
	sigset_t signal_set;
	
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGTTOU);
	
	sigprocmask(SIG_BLOCK, &signal_set, NULL);
}
