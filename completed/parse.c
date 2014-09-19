#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "arglist.h"
#include "parse.h"

// ******************************************************************************
// ******************************************************************************
// * Function Name: parseInput                                                  *
// * Description: This function takes in a line and parses that line based on   *
// * white space and newline characters. Then returns the parsing results in    *
// * argument list passed in.                                                   *
// * Parameters:                                                                *
// *    arglist* arglist : The location to store the arguments of the           *
// *                        parse line.                                         *
// * 	char* line       : The line to be parsed                                *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

bool parseInput(arglist* arg_list, char* line)
{
    char* token = strtok(line, " \n");

    while(token)
    {
        argListAdd(arg_list, token, arg_list->size);
        token = strtok(NULL, " \n");
    }

    // Error checking
    return inputErrorHandler(arg_list);
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: parseBG                                                     *
// * Description: This function takes in an argument list and returns wether    *
// * or not there is an & as the last non white space character in that         *
// * argument list. Additionally, it removes the & from the argument list.      *
// * Parameters:                                                                *
// *    arglist* arglist : The argument list to be checked for &                *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

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

// ******************************************************************************
// ******************************************************************************
// * Function Name: parseRedirs                                                 *
// * Description: This function takes in an argument list and returns through   *
// * its parameters the type of redirection, if any, and the stream to redirect *
// * to/from. Additionally, it deletes the redirection symbols and stream name  *
// * from the argument list.                                                    *
// * Parameters:                                                                *
// *    arglist* arglist      : The argument list to be checked for IO          *
// *                            redirects.                                      *
// *    bool* is_in_redir     : The location to store wether or not there is an *
// *                            input redirection "<" symbol in the argument    *
// *                            list.                                           *
// *    bool* is_out_redir    : The location to store wether or not there is an *
// *                            output redirection ">" symbol in the argument   *
// *                            list.                                           *
// *    bool* is_append_redir : The location to store wether or not there is an *
// *                            append redirection "<" symbol in the argument   *
// *                            list.                                           *
// *    char* redir file      : The location to return the name of the stream   *
// *                            to redirect to/from.                            *
// * Return: Void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void parseRedirs(arglist* arg_list, bool* is_in_redir, bool* is_out_redir, bool* is_append_redir, char* redir_file)
{
    int i = 0;
    for(; i < arg_list->size; i++)
    {
        if(strcmp(arg_list->args[i], "<") == 0) *is_in_redir = true;
        if(strcmp(arg_list->args[i], ">") == 0) *is_out_redir = true;
        if(strcmp(arg_list->args[i], ">>") == 0) *is_append_redir = true;

        if(*is_in_redir || *is_out_redir || *is_append_redir)
        {
            strcpy(redir_file, arg_list->args[i+1]);
            argListRemove(arg_list, i);
            argListRemove(arg_list, i);
        }
    }
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: parseIoacct                                                 *
// * Description: This function takes in an argument list and returns wether or *
// * not the command is an ioacct cmd and deletes the ioacct argument from the  *
// * argument list.                                                             *
// * Parameters:                                                                *
// *    arglist* arglist : The argument list to be checked for ioacct cmd.      *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

bool parseIoacct(arglist* arg_list)
{
    if (strcmp(arg_list->args[0], "ioacct") == 0)
    {
        argListRemove(arg_list, 0);
        return true;
    }
    return false;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: parsePipes                                                  *
// * Description: This function takes in an argument list and returns wether or *
// * not there are additional pipes and separating the two piped commands into  *
// * the parameters left_args and right_args                                    *
// * argument list.                                                             *
// * Parameters:                                                                *
// *    arglist* lef_args   : The argument list to be checked for pipes, and    *
// *                          the location to store the left side of the pipe.  *
// *    arglist* right_args : The argument list to store the right side of the  *
// *                          pipe.                                             *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

int parsePipes(arglist* arg_list1, arglist* arg_list2, arglist* arg_list3)
{
    size_t pipes = 0;

    int i = 0;
    for(; i < arg_list1->size; i++)
    {
        if(strcmp(arg_list1->args[i], "|") == 0)
        {
            pipes++;
            argListRemove(arg_list1, i);
        }
        if(pipes == 1) argListAdd(arg_list2, arg_list1->args[i], arg_list2->size);
        if(pipes == 2) argListAdd(arg_list3, arg_list1->args[i], arg_list3->size);
        if(pipes != 0) argListRemove(arg_list1, i--);
   }
   return pipes;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: inputErrorHandler                                           *
// * Description: This function takes in an argument list and first parses I/O  *
// * redirection, pipes, and &  symbols into separate arguments in the argument *
// * list, then checks for incorrectly placed symbols and returns wether or not *
// * the argument list is valid.                                                *
// * Parameters:                                                                *
// *    arglist* arglist : The argument list to be checked for input errors.    *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

bool inputErrorHandler(arglist* arg_list)
{
    // Check for no command entered
    if(arg_list->size == 0) return false;

    // Check for a command entered after ioacct
    if(arg_list->size == 1 && strcmp(arg_list->args[0], "ioacct") == 0)
    {
        printf("Usage: ioacct <command>\n");
        return false;
    }

    // Splits <, >, >>, and | into separate arguments
    int i = 0;
    while(i < arg_list->size)
    {
        int j = 0;
        while(arg_list->args[i][j] != '\0')
        {
            if(arg_list->args[i][j] == '<' ||
              (arg_list->args[i][j] == '>' && arg_list->args[i][j+1] == '>') ||
               arg_list->args[i][j] == '>' ||
               arg_list->args[i][j] == '|')
            {
                // If the operator is the already in an argument by itself, break to the next argument
                if(strcmp(arg_list->args[i], "<") == 0 ||
                   strcmp(arg_list->args[i], ">>") == 0 ||
                   strcmp(arg_list->args[i], ">") == 0 ||
                   strcmp(arg_list->args[i], "|") == 0)
                    break;

				int size = (arg_list->args[i][j+1] == '>') ? 3 : 2;

                char* substring;

                // Check for character at beginning
                if(j == 0)
                {
                    // Add first half of substring
                    substring = malloc(size);
                    strncpy(substring, arg_list->args[i], size-1);
                    substring[size-1] = '\0';
                    argListAdd(arg_list, substring, i++);
                    free(substring);

                    size_t len = strlen(arg_list->args[i]);

                    // Add second half of the substring
                    substring = malloc(len+(size-2));
                    strncpy(substring, arg_list->args[i]+size-1, len-(size-2));
                    argListAdd(arg_list, substring, i++);
                    free(substring);

                }
                // Check for character at end
                else if(j == strlen(arg_list->args[i])-(size-1))
                {
                    size_t len = strlen(arg_list->args[i]);

                    // Add first half of substring
                    substring = malloc(len-(size-2));
                    strncpy(substring, arg_list->args[i], len-(size-1));
                    substring[len-(size-1)] = '\0';
                    argListAdd(arg_list, substring, i++);
                    free(substring);

                    // Add second half of the substring
                    substring = malloc(size);
                    strncpy(substring, arg_list->args[i]+len-(size-1), size);
                    argListAdd(arg_list, substring, i++);
                    free(substring);
				}
				// Check for character in middle
                else
                {
                    size_t len = strlen(arg_list->args[i]);

                    // Add beginning of substring
                    substring = malloc(j+1);
                    strncpy(substring, arg_list->args[i], j);
                    substring[j] = '\0';
                    argListAdd(arg_list, substring, i++);
                    free(substring);

                    // Add middle of substring
                    substring = malloc(size);
                    strncpy(substring, arg_list->args[i]+j, size-1);
                    substring[size-1] = '\0';
                    argListAdd(arg_list, substring, i++);
                    free(substring);

                    // Add end of substring
                    substring = malloc(len-size);
                    strncpy(substring, arg_list->args[i]+j+(size-1), len-size);
                    argListAdd(arg_list, substring, i++);
                    free(substring);
                }

                argListRemove(arg_list, i--);
                break;
        }
        j++;
    }
    i++;
}

    int pipes = 0;

    // Check for incorrectly placed &, <, >, >>, and |
    i = 0;
    while(i < arg_list->size)
    {

        if(strcmp(arg_list->args[i], "<") == 0 ||
           strcmp(arg_list->args[i], ">") == 0 ||
           strcmp(arg_list->args[i], ">>") == 0 ||
           strcmp(arg_list->args[i], "|") == 0)
        {
            // Check for <, >, >> and | with invalid second operand
            if(i+1 >= arg_list->size || i-1 < 0 ||
                strcmp(arg_list->args[i+1], "<") == 0 ||
                strcmp(arg_list->args[i+1], ">") == 0 ||
                strcmp(arg_list->args[i+1], ">>") == 0 ||
                strcmp(arg_list->args[i+1], "|") == 0)
            {
                printf("Usage: <operand> %s <operand\n", arg_list->args[i]);
                return false;
            }
        }

    if(strcmp(arg_list->args[i], "|") == 0) pipes++;
    if(strcmp(arg_list->args[i], "<") == 0 && pipes > 0)
    {
        printf("Ambiguous command\n");
        return false;
    }


        int j = 0;
        while(arg_list->args[i][j] != '\0')
        {
            // Incorrectly placed &
            if(arg_list->args[i][j] == '&')
            {
                if(strcmp(arg_list->args[i], "&") != 0 &&
                  (arg_list->args[i][j+1] != '\0' || i != arg_list->size-1))
                {
                    printf("Usage: <command>&\n");
                    return false;
                }
            }

            j++;
        }
        i++;
    }

    return true;
}
