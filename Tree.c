#include <errno.h>

#include "Tree.h"
#include "HashMap.h"
#include <pthread.h>
#include <stdlib.h>

#define CHECK_PTR(x) do { if (!x) exit(0); } while(0)

Tree* tree_new() {
    Tree* result = (Tree*) malloc(sizeof(Tree));
    CHECK_PTR(result);

    pthread_mutex_init(&result->mutex, NULL);
    result->map = hmap_new();
    CHECK_PTR(result->map);
    
    result->parent = NULL;
    return result;
}

void tree_free(Tree *tree) {
    pthread_mutex_destroy(&tree->mutex);
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        tree_free(value);
    }
    hmap_free(tree->map);
    free(tree);
}