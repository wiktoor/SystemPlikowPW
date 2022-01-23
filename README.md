Program for my *Concurrent Programming* class. My goal was to create a file system with the following operations:
- `Tree* tree_new()` - creates a new tree with one empty folder `"/"`
- `void tree_free(Tree*)` - frees all the memory related to a tree
- `char* tree_list(Tree* tree, const char* path)` - lists all the contents of a folder (similiar to UNIX `ls` command)
- `int tree_create(Tree* tree, const char* path)` - creates a new, empty subfolder (similiar to UNIX `mkdir` command)
- `int tree_remove(Tree* tree, const char* path)` - removes a folder, if its empty (similiar to UNIX `rm` command)
- `int tree_move(Tree* tree, const char* source, const char* target)` - moves a folder to other place (similiar to UNIX `mv` command)

Additionally, all the operations, which can happen concurrently, should be done concurrently. An example of such operations is (writing in UNIX style) `ls /a/b/c/` and `rm /d/`.
