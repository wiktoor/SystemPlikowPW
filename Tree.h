#pragma once

#include <pthread.h>
#include "HashMap.h"

struct Tree {
    pthread_mutex_t mutex;
    HashMap* map;
    char* address;
};

typedef struct Tree Tree; // Let "Tree" mean the same as "struct Tree".

Tree* tree_new();

void tree_free(Tree*);

char* tree_list(Tree* tree, const char* path);

int tree_create(Tree* tree, const char* path);

int tree_remove(Tree* tree, const char* path);

int tree_move(Tree* tree, const char* source, const char* target);
