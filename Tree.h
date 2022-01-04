#pragma once

#include <pthread.h>
#include "HashMap.h"

#define READING 0
#define WRITING 1

struct Tree {
    // map to children of this Tree
    HashMap* map;
    pthread_mutex_t mutex;
    pthread_cond_t read_cond, write_cond, ref_cond;
    size_t read_wait, write_wait, working, ref_wait, ref;
    bool operation;
    // pointer to the parent (or is set to NULL if this tree is the root)
    struct Tree* parent;
};

typedef struct Tree Tree; // Let "Tree" mean the same as "struct Tree".

Tree* tree_new();

void tree_free(Tree*);

char* tree_list(Tree* tree, const char* path);

int tree_create(Tree* tree, const char* path);

int tree_remove(Tree* tree, const char* path);

int tree_move(Tree* tree, const char* source, const char* target);
