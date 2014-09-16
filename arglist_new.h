#ifndef ARGLIST_H
#define ARGLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct arglist{
        char** args;
        size_t size;
        size_t capacity;
};
typedef struct arglist arglist;

void argListCreate(arglist* arg_list, size_t capacity);
void argListDestroy(arglist* arg_list);
bool argListAdd(arglist* arg_list, const char* arg, size_t index);
bool argListRemove(arglist* arg_list, size_t index);
void argListPrint(arglist*);

#endif
