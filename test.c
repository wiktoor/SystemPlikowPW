#include "HashMap.h"
#include "Tree.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// void test_tree_create(Tree* tree, const char* path) {
//     printf("tree_create(%s): %d\n", path, tree_create(tree, path));
// }

// void test_tree_list(Tree* tree, const char* path) {
//     char* result = tree_list(tree, path);
//     if (result) {
//         printf("Dzieci wierzchołka \"%s\": %s\n", path, result);
//         free(result);
//     }
//     else {
//         printf("Wierzchołek \"%s\" nie istnieje\n", path);
//     }
// }

// void test_tree_remove(Tree* tree, const char* path) {
//     printf("tree_remove(%s): %d\n", path, tree_remove(tree, path));
// }

// void test_tree_move(Tree* tree, const char* source, const char* target) {
//     printf("tree_move(source = %s, target = %s): %d\n", source, target, tree_move(tree, source, target));
//     test_tree_list(tree, source);
//     test_tree_list(tree, target);
// }

int main() {
    Tree *tree = tree_new();

    // test_tree_create(tree, "/a/");
    // test_tree_create(tree, "/b/c/");
    // test_tree_create(tree, "/c/");
    // test_tree_create(tree, "/a/b/");
    // test_tree_create(tree, "/a/c/");
    // test_tree_create(tree, "/c/d/");

    // test_tree_list(tree, "/");
    // test_tree_list(tree, "/a/");
    // test_tree_list(tree, "/a/b/c/d/");

    // test_tree_remove(tree, "/a/");
    // test_tree_remove(tree, "/a/b/");

    // test_tree_list(tree, "/a/");
    // test_tree_list(tree, "/c/");

    // test_tree_move(tree, "/a/", "/c/");
    // test_tree_move(tree, "/a/", "/c/x/");

    tree_free(tree);
}