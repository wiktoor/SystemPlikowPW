#pragma once

#include <pthread.h>
#include "HashMap.h"

struct Tree {
    // map to children of this Tree
    HashMap* map;
    pthread_mutex_t mutex;
    pthread_cond_t read_cond, write_cond;
    size_t read_wait, write_wait, read_count, write_count;
    size_t subtree_count;
    pthread_cond_t subtree_cond;
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
