#include <errno.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h> // do testowania

#define CHECK_PTR(x) do { if (!x) exit(0); } while(0)
#define SUCCESS 0

void lock_subtree(Tree *tree) {
    pthread_mutex_lock(&tree->mutex);

    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        lock_subtree(value);
    }
}

void unlock_subtree(Tree *tree) {
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        unlock_subtree(value);
    }

    pthread_mutex_unlock(&tree->mutex);
}

Tree* find_tree(Tree *tree, const char* path, int* error) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = path;
    subpath = split_path(subpath, component);
    if (!subpath) return tree;

    Tree* subtree = (Tree*) hmap_get(tree->map, component);
    if (!subtree) {
        *error = ENOENT;
        return NULL;
    }
    return find_tree(subtree, subpath, error);
}

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

char* tree_list(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return NULL;

    int error = SUCCESS;
    lock_subtree(tree);
    Tree* node = find_tree(tree, path, &error);
    if (error != SUCCESS) {
        unlock_subtree(tree);
        return NULL;
    }
    char* result = make_map_contents_string(node->map);
    unlock_subtree(tree);

    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return EINVAL;

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_parent = make_path_to_parent(path, component);
    int error = SUCCESS;
    lock_subtree(tree);
    Tree* parent = find_tree(tree, path_to_parent, &error);
    if (error != SUCCESS) {
        free(path_to_parent);
        unlock_subtree(tree);
        return error;
    }

    Tree* new = tree_new();
    hmap_insert(parent->map, component, new);
    new->parent = parent;

    unlock_subtree(tree);

    free(path_to_parent);
    return SUCCESS;
}