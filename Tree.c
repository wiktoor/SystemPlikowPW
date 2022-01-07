#include <errno.h>

#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h> // strlen, strcmp
#include <stdio.h> // for test

#define CHECK_PTR(x) do { if (!x) exit(0); } while(0)
#define SUCCESS 0

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
    pthread_mutex_lock(&tree->mutex);

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

// read_unlocks the tree and all its predecessors
static void read_unlock_predecessors(Tree* tree) {
    read_unlock(tree);
    if (tree->parent) read_unlock_predecessors(tree->parent);
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

char* tree_list(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return NULL;

    Tree* node = read_lock_path(tree, path);
    if (!node) return NULL;

    // now we know that the node exists and is read_locked, thus, we can make operations on it
    // first, we unlock its predecessors (other than him), so other processes can use them
    if (node->parent) read_unlock_predecessors(node->parent);

    char* result = make_map_contents_string(node->map);
    read_unlock(node);
    return result;
}

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

static bool is_root(const char* path) {
    return !strcmp(path, "/");
}

int tree_create(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return EINVAL;
    if (is_root(path)) return EEXIST;

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

int tree_remove(Tree* tree, const char* path) {
    if (!is_path_valid(path)) return EINVAL;

    char component[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_parent = make_path_to_parent(path, component);
    if (!path_to_parent) return EBUSY;
    Tree* parent = read_write_lock_path(tree, path_to_parent);
    free(path_to_parent);
    if (!parent) return ENOENT;
    
    // now we know that parent exists and is write_locked, thus, we can make operations on it
    // first, we unlock its predecessors (other than him), so other processes can use them
    if (parent->parent) read_unlock_predecessors(parent->parent);

    Tree* node = hmap_get(parent->map, component);
    if (!node) {
        write_unlock(parent);
        return ENOENT;
    }

    write_lock(node);
    if (hmap_size(node->map)) {
        write_unlock(node);
        write_unlock(parent);
        return ENOTEMPTY;
    }

    hmap_remove(parent->map, component);
    tree_free(node);

    write_unlock(parent);

    return SUCCESS;
}

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

static char* rest_path(const char* path, const char* successor_path) {
    size_t path_length = strlen(path), successor_path_length = strlen(successor_path);
    size_t rest_path_length = successor_path_length - path_length + 1;
    char* result = malloc(rest_path_length + 1);
    CHECK_PTR(result);
    memcpy(result, successor_path + path_length - 1, rest_path_length);
    result[rest_path_length] = '\0';
    return result;
}

// read_unlocks the tree and all its predecessors
static void read_unlock_predecessors_until_root(Tree* tree, Tree* root) {
    if (tree == root) return;
    read_unlock(tree);
    if (tree->parent) read_unlock_predecessors_until_root(tree->parent, root);
}

static Tree* read_write_lock_path_root_excluding(Tree* tree, const char* path, Tree* root) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = path;
    subpath = split_path(subpath, component);

    if (tree != root) {
        if (!subpath) {
            write_lock(tree);
            return tree;
        }
        read_lock(tree);
    }

    Tree* subtree = (Tree*) hmap_get(tree->map, component);
    if (!subtree) {
        read_unlock_predecessors_until_root(tree, root);
        return NULL;
    }
    return read_write_lock_path_root_excluding(subtree, subpath, root);
}

static void write_lock_subtree(Tree* tree, bool lock_root) {
    if (lock_root) write_lock(tree);

    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        write_lock_subtree(value, true);
    }
}

static void write_unlock_subtree(Tree* tree, bool unlock_root) {
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->map);
    while (hmap_next(tree->map, &it, &key, &value)) {
        write_unlock_subtree(value, true);
    }

    if (unlock_root) write_unlock(tree);
}

static char* find_last_common_predecessor(const char* path1, const char* path2) {
    size_t len1 = strlen(path1), len2 = strlen(path2);
    size_t len = len1 < len2 ? len1 : len2;
    size_t cnt = 0;
    while (path1[cnt] == path2[cnt] && cnt < len) cnt++;
    char* res = malloc(cnt + 1);
    CHECK_PTR(res);
    memcpy(res, path1, cnt);
    res[cnt] = '\0';
    return res;
}

int tree_move(Tree* tree, const char* source, const char* target) {
    if (!is_path_valid(source) || !is_path_valid(target)) return EINVAL;

    // corner case for tests: if source == "/"
    if (is_root(source)) return EBUSY;

    // corner case for tests: if target == "/"
    if (is_root(target)) return EEXIST;

    // corner case for tests: if target is successor of source
    if (is_successor(source, target)) return -1;

    // important case
    if (!strcmp(source, target)) {
        Tree* source_node = read_lock_path(tree, source); // do we need to read_write lock it?
        if (source_node) {
            read_unlock_predecessors(source_node);
            return SUCCESS;
        }
        else return ENOENT;
    }

    // important case
    if (is_successor(target, source)) {
        Tree* source_node = read_lock_path(tree, source); // do we need to read_write lock it?
        if (source_node) {
            read_unlock_predecessors(source_node);
            return EEXIST;
        }
        else return ENOENT;
    }

    char source_name[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_source_parent = make_path_to_parent(source, source_name);
    if (!path_to_source_parent) return EBUSY;

    char target_name[MAX_FOLDER_NAME_LENGTH + 1];
    char* path_to_target_parent = make_path_to_parent(target, target_name);
    if (!path_to_target_parent) return EEXIST;

    // find lcp
    char* lcp_path = find_last_common_predecessor(path_to_source_parent, path_to_target_parent);
    Tree* lcp = read_write_lock_path(tree, lcp_path);
    if (!lcp) {
        free(lcp_path);
        return ENOENT;
    }

    if (lcp->parent) read_unlock_predecessors(lcp->parent);

    // find source_parent
    char* lcp_to_source_parent = rest_path(lcp_path, path_to_source_parent);
    Tree* source_parent = strlen(lcp_to_source_parent) > 1 ? read_write_lock_path_root_excluding(lcp, lcp_to_source_parent, lcp) : lcp;

    free(lcp_to_source_parent);
    free(path_to_source_parent);
    if (!source_parent) {
        free(lcp_path);
        write_unlock(lcp);
        return ENOENT;
    }

    if (source_parent != lcp && source_parent->parent) read_unlock_predecessors_until_root(source_parent->parent, lcp);

    // find source_node
    Tree* source_node = hmap_get(source_parent->map, source_name);
    if (!source_node) {
        free(lcp_path);
        if (source_parent != lcp) write_unlock(source_parent);
        write_unlock(lcp);
        return ENOENT;
    }
    write_lock(source_node);

    // if source and target are the same
    if (!strcmp(source, target)) {
        free(lcp_path);
        write_unlock(source_node);
        if (source_parent != lcp) write_unlock(source_parent);
        write_unlock(lcp);
        return SUCCESS;
    }
    
    // find target parent
    char* lcp_to_target_parent = rest_path(lcp_path, path_to_target_parent);
    Tree* target_parent = strlen(lcp_to_target_parent) > 1 ? read_write_lock_path_root_excluding(lcp, lcp_to_target_parent, lcp) : lcp;
    
    free(lcp_path);
    free(lcp_to_target_parent);
    free(path_to_target_parent);
    if (!target_parent) {
        write_unlock(source_node);
        if (source_parent != lcp) write_unlock(source_parent);
        write_unlock(lcp);
        return ENOENT;
    }

    if (target_parent != lcp && target_parent->parent) read_unlock_predecessors_until_root(target_parent->parent, lcp);

    // check if target already exists
    if (hmap_get(target_parent->map, target_name)) {
        write_unlock(source_node);
        if (source_parent != lcp) write_unlock(source_parent);
        if (target_parent != lcp) write_unlock(target_parent);
        write_unlock(lcp);
        return EEXIST;
    }

    // we're good to go
    write_lock_subtree(source_node, false);
    hmap_remove(source_parent->map, source_name);
    hmap_insert(target_parent->map, target_name, source_node);
    source_node->parent = target_parent;

    write_unlock_subtree(source_node, true);
    if (source_parent != lcp) write_unlock(source_parent);
    if (target_parent != lcp) write_unlock(target_parent);
    write_unlock(lcp);

    return SUCCESS;
}