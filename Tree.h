#pragma once

#include <pthread.h>
#include "HashMap.h"

/* we return that when everything goes as expected */
#define SUCCESS 0

/* error that occurs when we want to move a folder to its successor */
#define ESUCCESSOR -1

typedef struct Tree Tree; // Let "Tree" mean the same as "struct Tree".

/* creates new tree with just one empty folder "/" 
   returns a pointer to the new tree */
Tree* tree_new();

/* frees all the memory related to this tree's subtree (this tree including) */
void tree_free(Tree*);

/* lists all the contents of the folder 
   note that the result should be then free'd by the user 
   returns NULL if path is incorrect or taget folder doesn't exist */
char* tree_list(Tree* tree, const char* path);

/* creates new, empty subfolder 
    returns EINVAL if the path is incorrect 
    returns EEXIST if the folder already exists 
    returns ENOENT if the folder's parent doesn't exist 
    returns SUCCESS otherwise */
int tree_create(Tree* tree, const char* path);

/* removes a folder, if it's empty 
   returns EINVAL if the path is incorrect 
   returns ENOTEMPTY if the folder isn't empty
   returns ENOENT if the folder doesn't exist 
   returns EBUSY if the path points to the root
   returns SUCCESS otherwise */
int tree_remove(Tree* tree, const char* path);

/* moves folder source (and all its subfolders) to target 
   returns EINVAL if the path is incorrect 
   returns ESUCCESSOR if target is a successor of source
   returns ENOENT if source or target's parent don't exist
   returns EEXIST if target already exists 
   returns EBUSY if source path points to the root
   returns SUCCESS otherwise */
int tree_move(Tree* tree, const char* source, const char* target);
