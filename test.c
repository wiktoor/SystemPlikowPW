#include "HashMap.h"
#include "Tree.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void test_tree_create(Tree* tree, const char* path) {
    printf("%d\n", tree_create(tree, path));
}

void test_tree_list(Tree* tree, const char* path) {
    char* result = tree_list(tree, path);
    if (result) {
        printf("Dzieci wierzchołka \"%s\": %s\n", path, result);
        free(result);
    }
    else {
        printf("Wierzchołek \"%s\" nie istnieje\n", path);
    }
}

int main() {
    Tree *tree = tree_new();

    test_tree_create(tree, "/a/");
    test_tree_create(tree, "/b/c/");
    test_tree_create(tree, "/c/");
    test_tree_create(tree, "/a/b/");
    test_tree_create(tree, "/a/c/");

    test_tree_list(tree, "/");
    test_tree_list(tree, "/a/");
    test_tree_list(tree, "/a/b/c/d/");

    tree_free(tree);
}