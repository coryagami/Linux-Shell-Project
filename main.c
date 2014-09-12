#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>

int parseInput(char* args[], char* line);
char* cmdExist(char* cmd, char* path);

int main()
{
	system("clear");		/*	might not be allowed */
	
	char host_name[64];
	char* user_name = (char* ) malloc(64);
	char curr_dir[1024];
	
	char* args0 = (char* ) malloc(12);
	char* args1 = (char* ) malloc(12);
	char* args2 = (char* ) malloc(12);
	char* args3 = (char* ) malloc(12);
	char* args[] = {args0, args1, args2, args3, (char *)NULL};			// FIGURE THIS OUT!
	char line[256];
	
	/* maybe put below 2 functions inside loop? */
	gethostname(host_name, sizeof(host_name));
	user_name = getlogin();
	
	while(1)
	{	
		args[0] = args[1] = args[2] = args[3] = NULL;
		getcwd(curr_dir, sizeof(curr_dir));
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);
		
		fgets(line, sizeof(line), stdin);
		int numArgs = parseInput(args, line);
		if(numArgs == 0)
			continue;
		// printf("\n%s %s %s %s\n", args[0], args[1], args[2], args[3]);

		if(strcmp(args[0], "cd") == 0 || strcmp(args[0], "ioacct") == 0 || strcmp(args[0], "exit") == 0) {
			if (strcmp(args[0], "cd") == 0) {
				chdir(args[1]);
			}
			else if (strcmp(args[0], "ioacct") == 0) {
				// do ioacct
			}
			else if (strcmp(args[0], "exit") == 0) {
				if(args[1] == NULL)
					return 0;
				else
					return atoi(args[1]);
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
		
		// freeing args memory accordingly
		int x=0;
		for(;x<numArgs-1; x++) {
			free(args[x]); 
		}
	}
}	

int parseInput(char* args[], char* line)
{
	char* token;
	token = strtok(line, " \n");
	if(token == NULL)
		return 0;
	args[0] = strdup(token);
	int i=1;
	while(token = strtok(NULL, " ")) {
		args[i] = strdup(token);
		i++;
	} 
	size_t ln = strlen(args[i-1])-1; 
	if(args[i-1][ln] == '\n') 
		args[i-1][ln] = '\0';
	return i+1;
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
