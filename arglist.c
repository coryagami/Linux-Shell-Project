#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "arglist.h"

void argListCreate(arglist* arg_list, size_t capacity) {
        arg_list->size = 0;
        arg_list->capacity = capacity;
        arg_list->args = malloc(arg_list->capacity * sizeof(char*));
}

void argListDestroy(arglist* arg_list) {
        int i = 0;
        for(; i < arg_list->size; i++)
                free(arg_list->args[i]);
        free(arg_list->args);
}

bool argListAdd(arglist* arg_list, char* arg) {
        if (arg_list->size >= arg_list->capacity) {
                arg_list->capacity *= 2;
                arg_list->args = realloc(arg_list->args, arg_list->capacity * sizeof(char*));
        }

        arg_list->args[arg_list->size] = malloc(strlen(arg) + 1);
        strcpy(arg_list->args[arg_list->size++], arg);
        return true;
}

bool argListRemove(arglist* arg_list, size_t index) {
        if (index < 0 || index >= arg_list->size)
                return false;
        int i = 0;
        for(;i < arg_list->size-1; i++) {
                if (i == index) free(arg_list->args[i]);
                if (i >= index) arg_list->args[i] = arg_list->args[i+1];
        }
        arg_list->size--;
        return true;
}

