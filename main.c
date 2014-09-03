#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
	system("clear");		/*	might not be allowed */
	
	char* host_name = (char* ) malloc(64);
	char* user_name = (char* ) malloc(64);
	char curr_dir[1024];
	
	char* args0 = (char* ) malloc(12);
	char* args1 = (char* ) malloc(12);
	char* args2 = (char* ) malloc(12);
	char* args3 = (char* ) malloc(12);
	char* args[] = {args0, args1, args2, args3, (char *)NULL};
	char line[256];
	char* token;
	char* envpath;
	
	/* maybe put below 2 functions inside loop? */
	gethostname(host_name, sizeof(host_name));
	user_name = getlogin();
	
	while(1)			// maybe reset args after each loop?
	{
		getcwd(curr_dir, sizeof(curr_dir));
		printf("%s@%s:%s $ ", host_name, user_name, curr_dir);
		gets(line);
		
		token = strtok(line, " ");
		args[0] = token;
		int i=1;
		while(token != NULL && i < 4) {
			token = strtok(NULL, " ");
			args[i] = token;
			i++;
		}
		// printf("\n%s %s %s %s\n", args[0], args[1], args[2], args[3]);


		if(strcmp(args[0], "cd") == 0 || strcmp(args[0], "ioacct") == 0 || strcmp(args[0], "exit") == 0) {
			if (strcmp(args[0], "cd") == 0) {
				chdir(args[1]);
			}
			else if (strcmp(args[0], "ioacct") == 0) {
				// do ioacct
			}
			else if (strcmp(args[0], "exit") == 0) {
				return atoi(args[1]);
			}
		}
		else {
			envpath = getenv("PATH");
			// SEARCH FOR COMMAND HERE
			printf("PATH: %s\n", envpath);
			printf("%s command not supported yet\n", args[0]);
		}
	}
}	
