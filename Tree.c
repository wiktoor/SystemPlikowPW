#include <errno.h>

#include "Tree.h"
#include "HashMap.h"
#include <pthread.h>
#include <stdlib.h>

Tree* tree_new() {
    Tree* result = (Tree*) malloc(sizeof(Tree));
    pthread_mutex_init(&result->mutex, NULL);
    result->map = hmap_new();
    result->address = (char*) malloc(2 * sizeof(char));
    result->address[0] = '/';
    result->address[1] = '\0';
    return result;
}