#include <errno.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h> // strlen, strcmp

#define CHECK_PTR(x) do { if (!x) exit(0); } while(0)
#define SUCCESS 0

static bool is_successor(const char* path, const char* successor_path) {
    size_t length = strlen(path);
    if (length >= strlen(successor_path)) return false;
    char* short_path = malloc(length + 1);
    CHECK_PTR(short_path);
    memcpy(short_path, successor_path, length);
    short_path[length] = '\0';
    bool result = !strcmp(short_path, path);
    free(short_path);
    return result;
}

static void lock_subtree(Tree *tree) {
    pthread_mutex_lock(&tree->mutex);

    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        lock_subtree(value);
    }
}

static void unlock_subtree(Tree *tree) {
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        unlock_subtree(value);
    }

    pthread_mutex_unlock(&tree->mutex);
}

static Tree* tree_find(Tree *tree, const char* path, int* error) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = path;
    subpath = split_path(subpath, component);
    if (!subpath) return tree;

    Tree* subtree = (Tree*) hmap_get(tree->map, component);
    if (!subtree) {
        if (error) *error = ENOENT;
        return NULL;
    }
    return tree_find(subtree, subpath, error);
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
    Tree* node = tree_find(tree, path, &error);
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
    Tree* node = tree_find(tree, path, NULL);
    if (node) {
        free(path_to_parent);
        unlock_subtree(tree);
        return EEXIST;
    }

    Tree* parent = tree_find(tree, path_to_parent, &error);
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

int tree_remove(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return EINVAL;
    char* path_to_parent = make_path_to_parent(path, NULL);
    if (!path_to_parent) return EBUSY;
    else free(path_to_parent);

    int error = SUCCESS;
    lock_subtree(tree);
    Tree* node = tree_find(tree, path, &error);
    if (error != SUCCESS) {
        unlock_subtree(tree);
        return error;
    }

    if (hmap_size(node->map)) {
        unlock_subtree(tree);
        return ENOTEMPTY;
    }

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    char* parent_path = make_path_to_parent(path, component);
    free(parent_path);

    hmap_remove(node->parent->map, component);
    tree_free(node);

    unlock_subtree(tree);

    return SUCCESS;
}

int tree_move(Tree* tree, const char* source, const char* target) {
    if (!is_path_valid(source) || !is_path_valid(target)) return EINVAL;

    // corner case for tests: if source == "/"
    if (strlen(source) == 1 && *source == '/') return EBUSY;

    // corner case for tests: if target == "/"
    if (strlen(target) == 1 && *target == '/') return EEXIST;

    // corner case for tests: if target is successor of source
    if (is_successor(source, target)) return -1;

    // find the name of source folder
    char source_name[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_source_parent = make_path_to_parent(source, source_name);
    if (!path_to_source_parent) {
        return EBUSY;
    }
    free(path_to_source_parent);

    int error = SUCCESS;
    lock_subtree(tree);

    // find tree_source
    Tree* tree_source = tree_find(tree, source, &error);
    if (error != SUCCESS) {
        unlock_subtree(tree);
        return error;
    }

    // corner case for tests: if source == target
    if (!strcmp(source, target)) {
        unlock_subtree(tree);
        return SUCCESS;
    }

    // check if target already exists
    Tree* target_tree = tree_find(tree, target, NULL);
    if (target_tree) {
        unlock_subtree(tree);
        return EEXIST;
    }

    // find target's parent and name
    char target_name[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_target_parent = make_path_to_parent(target, target_name);
    Tree* target_parent = tree_find(tree, path_to_target_parent, &error);
    if (path_to_target_parent) free(path_to_target_parent);
    if (error != SUCCESS) {
        unlock_subtree(tree);
        return error;
    }

    // remove source from their parent's map
    hmap_remove(tree_source->parent->map, source_name);

    // insert tree_source into target's parent's map
    hmap_insert(target_parent->map, target_name, tree_source);

    // set tree_source's parent as new parent
    tree_source->parent = target_parent;

    unlock_subtree(tree);

    return SUCCESS;
}