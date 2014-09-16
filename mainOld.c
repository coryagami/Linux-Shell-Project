#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/types.h>

int parseInput(char* args[], char* line);
char* cmdExist(char* cmd, char* path);
void doIoacct(pid_t pid);

int main()
{
	system("clear");		/*	might not be allowed */

	char host_name[64];
	char* user_name = (char* ) malloc(64);
	char curr_dir[1024];
	char last_dir[1024];

	char* args0 = (char* ) malloc(12);
	char* args1 = (char* ) malloc(12);
	char* args2 = (char* ) malloc(12);
	char* args3 = (char* ) malloc(12);
	char* args[] = {args0, args1, args2, args3, (char *)NULL};			// FIGURE THIS OUT!
	char line[256];
	int ioat;
	int background;


	while(1)
	{
		ioat = 0;
		background = 0;
		args[0] = args[1] = args[2] = args[3] = NULL;
		gethostname(host_name, sizeof(host_name));
		user_name = getlogin();
		getcwd(curr_dir, sizeof(curr_dir));
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);

		fgets(line, sizeof(line), stdin);
		int numArgs = parseInput(args, line);
		if(numArgs == 0)
			continue;
		// printf("\n%s %s %s %s\n", args[0], args[1], args[2], args[3]);

		if(strcmp(args[0], "ioacct") == 0) {
			int i=0;
			for(;i<numArgs;i++) {
				args[i] = args[i+1];
			}
			ioat = 1;
		}

		int i=3;
		for(; i>=0; i--) {
			if(args[i] != NULL) {
				int last_len = strlen(args[i]);
				if(args[i][last_len-1] == '&' && last_len > 1) {
					args[i][last_len-1] = '\0';
					background = 1; printf("BGBGBG");
				}
				else if(args[i][last_len-1] == '&' && last_len == 1) {
					args[i] = NULL;
					background = 1; printf("BGBGBG");
				}
				break;
			}
		} printf("\n%s %s %s %s\n", args[0], args[1], args[2], args[3]);

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
				strcpy(last_dir, curr_dir);
			}
			else if (strcmp(args[0], "exit") == 0) {
				if(args[1] == NULL)
					return 0;
				else
					return atoi(args[1]);
			}
			if(ioat == 1)
				doIoacct(getpid());
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
				else if (background == 1) {
					printf("back!");
					pid_t child_finished;
					child_finished = waitpid(-1, (int *)NULL, WNOHANG);

					if(ioat == 1)
						doIoacct(child);	// <-- WEIRDDDDDDDDDD
				}
				else {
					pid_t child_finished;
					child_finished = waitpid(-1, (int *)NULL, 0);

					if(ioat == 1)
						doIoacct(child);
				}
			}

			// IF NOT FOUND --------------------------------------------
			else {
				printf("Command '%s' NOT FOUND!\n", args[0]);
			}
		}

		// freeing args memory accordingly
		int x=0;
		for(;x<numArgs-1; x++) {		// FIX THIS when 'man' then 'q' it fails
			// printf("1");
			free(args[x]);
			// printf("2");
		}
	}
}

void doIoacct(pid_t pid)
{
	char pid_c[10];
	char filename[] = "/proc/";
	snprintf(pid_c, 10, "%d", (int)pid);
	strcat(filename, pid_c);
	strcat(filename, "/io");

	printf("%s\n", filename);

	FILE* fp;
	fp = fopen(filename, "r");
	if(fp == NULL) {
		printf("Cant open io file at %s, errno: %d\n", filename, errno);
	}
	else {
		char line[64];
		int i=0;
		for(;i<6; i++) {
			fgets(line, sizeof(line), fp);
			if(i == 4 || i == 5)
				printf("%s", line);
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
