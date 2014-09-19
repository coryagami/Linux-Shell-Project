#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "arglist.h"

void argListPrint(arglist* arg_list)
{
	int i = 0;
	for(; i < arg_list->size; i++)
	{
		printf("arg %d: %s\n", i, arg_list->args[i]);
	}
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: argListCreate                                               *
// * Description: This function allocates memory for a new argument list        *
// * Parameters:                                                                *
// *    arglist* arglist : The location to allocate the memory.                 *
// *    site_t capacity  : Initial argument capacity.                           *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void argListCreate(arglist* arg_list, size_t capacity)
{
        arg_list->size = 0;
        arg_list->capacity = capacity;
        arg_list->args = malloc(arg_list->capacity * sizeof(char*));

	int i = 0;
	for(; i < arg_list->capacity; i++)
		arg_list->args[i] = NULL;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: argListDestroy                                              *
// * Description: This deallocates an argument list.                            *
// * Parameters:                                                                *
// *    arglist* arglist : The location of the argument list to be deallocated. *
// * Return: void                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

void argListDestroy(arglist* arg_list)
{
        int i = 0;
        for(; i < arg_list->size; i++)
                free(arg_list->args[i]);
        free(arg_list->args);
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: argListAdd                                                  *
// * Description: This function takes in an argument and adds it to the         *
// * argument list                                                              *
// * Parameters:                                                                *
// *    arglist* arglist : The argument list to add the argument.         	*
// *    const char* arg  : The argument to be inserted.                         *
// *    size_t index     : The index to insert the item				*
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

bool argListAdd(arglist* arg_list, const char* arg, size_t index)
{
        if (arg_list->size >= arg_list->capacity-1)
	{
                arg_list->capacity *= 2;
                arg_list->args = realloc(arg_list->args, arg_list->capacity * sizeof(char*));

		int i = arg_list->size;
		for(; i < arg_list->capacity; i++)
			arg_list->args[i] = NULL;
        }

	int i = arg_list->size;
	for(;i > index; i--)
		arg_list->args[i] = arg_list->args[i-1];

        arg_list->args[index] = malloc(strlen(arg) + 1);
        strcpy(arg_list->args[index], arg);
	arg_list->size++;
        return true;
}

// ******************************************************************************
// ******************************************************************************
// * Function Name: parseInput                                                  *
// * Description: This function takes in a line and parses that line based on   *
// * white space and newline characters. Then returns the parsing results in    *
// * argument list passed in.                                                   *
// * Parameters:                                                                *
// *    arglist* arglist : The location to store the arguments of the           *
// *                        parse line.                                         *
// *    char* line       : The line to be parsed                                *
// * Return: bool                                                               *
// *                                                                            *
// ******************************************************************************
// ******************************************************************************

bool argListRemove(arglist* arg_list, size_t index)
{
        if (index < 0 || index >= arg_list->size)
                return false;

        free(arg_list->args[index]);

        int i = index;
        for(;i < arg_list->size-1; i++)
                arg_list->args[i] = arg_list->args[i+1];

	arg_list->args[--arg_list->size] = NULL;
        return true;
}

