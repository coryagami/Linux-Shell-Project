#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "arglist.h"

// *********************************************************
// *********************************************************
// * File Name: parse.h                                    *
// * Description: The declarations for parse.c.            *
// *                                                       *
// *********************************************************
// *********************************************************

int parsePipes(arglist* arg_list1, arglist* arg_list2, arglist* arg_list3);
bool parseInput(arglist* arg_list, char* line);
bool parseBG(arglist* arg_list);
void parseRedirs(arglist* arg_list, bool* is_in_redir, bool* is_out_redir, bool* is_append_redir, char* redir_file);
bool parseIoacct(arglist* arg_list);
bool inputErrorHandler(arglist* arg_list);

#endif
