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

static void read_lock(Tree* tree) {
    pthread_mutex_lock(&tree->mutex);

    if (tree->operation == WRITING || tree->write_wait) {
        tree->read_wait++;
        pthread_cond_wait(&tree->read_cond, &tree->mutex);
        tree->read_wait--;
    }

    tree->working++;
    pthread_cond_signal(&tree->read_cond);

    pthread_mutex_unlock(&tree->mutex);
}

static void read_unlock(Tree* tree) {
    pthread_mutex_unlock(&tree->mutex);

    if (--tree->working == 0) pthread_cond_signal(&tree->write_cond);

    pthread_mutex_unlock(&tree->mutex);
}

static void write_lock(Tree* tree) {
    pthread_mutex_lock(&tree->mutex);

    if (tree->working) {
        tree->write_wait++;
        pthread_cond_wait(&tree->write_cond, &tree->mutex);
        tree->write_wait--;
    }

    tree->operation = WRITING;
    tree->working++;

    pthread_mutex_unlock(&tree->mutex);
}

static void write_unlock(Tree* tree) {
    pthread_mutex_lock(&tree->mutex);

    tree->operation = READING;
    tree->working--;

    if (tree->read_wait) pthread_cond_signal(&tree->read_cond);
    else pthread_cond_signal(&tree->write_cond);

    pthread_mutex_unlock(&tree->mutex);
}

// static void lock_subtree(Tree *tree) {
//     pthread_mutex_lock(&tree->mutex);

//     const char* key = NULL;
//     void* value = NULL;
//     HashMapIterator it = hmap_iterator(tree->map);
//     while (hmap_next(tree->map, &it, &key, &value)) {
//         lock_subtree(value);
//     }
// }

// static void unlock_subtree(Tree *tree) {
//     const char* key = NULL;
//     void* value = NULL;
//     HashMapIterator it = hmap_iterator(tree->map);
//     while (hmap_next(tree->map, &it, &key, &value)) {
//         unlock_subtree(value);
//     }

//     pthread_mutex_unlock(&tree->mutex);
// }

// static Tree* tree_find(Tree *tree, const char* path, int* error) {
//     char component[MAX_FOLDER_NAME_LENGTH + 1];
//     const char* subpath = path;
//     subpath = split_path(subpath, component);
//     if (!subpath) return tree;

//     Tree* subtree = (Tree*) hmap_get(tree->map, component);
//     if (!subtree) {
//         if (error) *error = ENOENT;
//         return NULL;
//     }
//     return tree_find(subtree, subpath, error);
// }

// read_unlocks the tree and all its predecessors
static void read_unlock_predecessors(Tree* tree) {
    read_unlock(tree);
    if (tree->parent) read_unlock(tree->parent);
}

// read_locks whole path, returns the node that the path points to
// if such path doesn't exist, returns NULL and rollbacks all read_locks
static Tree* read_lock_path(Tree* tree, const char* path) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = path;
    subpath = split_path(subpath, component);

    read_lock(tree);
    if (!subpath) return tree;

    Tree* subtree = (Tree*) hmap_get(tree->map, component);
    if (!subtree) {
        read_unlock_predecessors(tree);
        return NULL;
    }
    return read_lock_path(subtree, subpath);
}

Tree* tree_new() {
    Tree* result = (Tree*) malloc(sizeof(Tree));
    CHECK_PTR(result);

    result->map = hmap_new();
    CHECK_PTR(result->map);

    pthread_mutex_init(&result->mutex, NULL);
    pthread_cond_init(&result->read_cond, NULL);
    pthread_cond_init(&result->write_cond, NULL);

    result->read_wait = result->write_wait = result->working = 0;
    result->operation = READING;
    result->parent = NULL;

    return result;
}

void tree_free(Tree *tree) {
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        tree_free(value);
    }
    hmap_free(tree->map);
    pthread_mutex_destroy(&tree->mutex);
    pthread_cond_destroy(&tree->read_cond);
    pthread_cond_destroy(&tree->write_cond);
    free(tree);
}

// char* tree_list(Tree* tree, const char* path) {
//     if (!is_path_valid(path)) return NULL;

//     int error = SUCCESS;
//     lock_subtree(tree);
//     Tree* node = tree_find(tree, path, &error);
//     if (error != SUCCESS) {
//         unlock_subtree(tree);
//         return NULL;
//     }
//     char* result = make_map_contents_string(node->map);
//     unlock_subtree(tree);

//     return result;
// }

// read_locks whole path, except for the last node, which is write_locked
// returns the node that the path points to
// if such path doesn't exist, returns NULL and rollbacks all read_locks
static Tree* read_write_lock_path(Tree* tree, const char* path) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = path;
    subpath = split_path(subpath, component);

    if (!subpath) {
        write_lock(tree);
        return tree;
    }
    read_lock(tree);

    Tree* subtree = (Tree*) hmap_get(tree->map, component);
    if (!subtree) {
        read_unlock_predecessors(tree);
        return NULL;
    }
    return read_write_lock_path(subtree, subpath);
}

int tree_create(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return EINVAL;

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_parent = make_path_to_parent(path, component);
    Tree* parent = read_write_lock_path(tree, path_to_parent);
    free(path_to_parent);
    if (!parent) return ENOENT;

    // now we know that parent exists and is write_locked, thus, we can make operations on it
    // first, we unlock its predecessors (other than him), so other processes can use them
    if (parent->parent) read_unlock_predecessors(parent->parent);
    
    if (hmap_get(parent->map, component)) {
        write_unlock(parent);
        return EEXIST;
    }

    Tree* new = tree_new();
    hmap_insert(parent->map, component, new);
    new->parent = parent;

    write_unlock(parent);
    return SUCCESS;
}

// int tree_remove(Tree* tree, const char* path) {
//     if (!is_path_valid(path)) return EINVAL;
//     char* path_to_parent = make_path_to_parent(path, NULL);
//     if (!path_to_parent) return EBUSY;
//     else free(path_to_parent);

//     int error = SUCCESS;
//     lock_subtree(tree);
//     Tree* node = tree_find(tree, path, &error);
//     if (error != SUCCESS) {
//         unlock_subtree(tree);
//         return error;
//     }

//     if (hmap_size(node->map)) {
//         unlock_subtree(tree);
//         return ENOTEMPTY;
//     }

//     char component[MAX_FOLDER_NAME_LENGTH + 1];
//     char* parent_path = make_path_to_parent(path, component);
//     free(parent_path);

//     hmap_remove(node->parent->map, component);
//     tree_free(node);

//     unlock_subtree(tree);

//     return SUCCESS;
// }

// int tree_move(Tree* tree, const char* source, const char* target) {
//     if (!is_path_valid(source) || !is_path_valid(target)) return EINVAL;

//     // corner case for tests: if source == "/"
//     if (strlen(source) == 1 && *source == '/') return EBUSY;

//     // corner case for tests: if target == "/"
//     if (strlen(target) == 1 && *target == '/') return EEXIST;

//     // corner case for tests: if target is successor of source
//     if (is_successor(source, target)) return -1;

//     // find the name of source folder
//     char source_name[MAX_FOLDER_NAME_LENGTH + 1];
//     char* path_to_source_parent = make_path_to_parent(source, source_name);
//     if (!path_to_source_parent) {
//         return EBUSY;
//     }
//     free(path_to_source_parent);

//     int error = SUCCESS;
//     lock_subtree(tree);

//     // find tree_source
//     Tree* tree_source = tree_find(tree, source, &error);
//     if (error != SUCCESS) {
//         unlock_subtree(tree);
//         return error;
//     }

//     // corner case for tests: if source == target
//     if (!strcmp(source, target)) {
//         unlock_subtree(tree);
//         return SUCCESS;
//     }

//     // check if target already exists
//     Tree* target_tree = tree_find(tree, target, NULL);
//     if (target_tree) {
//         unlock_subtree(tree);
//         return EEXIST;
//     }

//     // find target's parent and name
//     char target_name[MAX_FOLDER_NAME_LENGTH + 1];
//     char* path_to_target_parent = make_path_to_parent(target, target_name);
//     Tree* target_parent = tree_find(tree, path_to_target_parent, &error);
//     if (path_to_target_parent) free(path_to_target_parent);
//     if (error != SUCCESS) {
//         unlock_subtree(tree);
//         return error;
//     }

//     // remove source from their parent's map
//     hmap_remove(tree_source->parent->map, source_name);

//     // insert tree_source into target's parent's map
//     hmap_insert(target_parent->map, target_name, tree_source);

//     // set tree_source's parent as new parent
//     tree_source->parent = target_parent;

//     unlock_subtree(tree);

//     return SUCCESS;
// }