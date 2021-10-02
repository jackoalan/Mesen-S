#define	_SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

#include "tsearch_path.h"

static void
tdestroy_recurse(const posix_tnode *root, void (*free_node)(void *nodep))
{
	if (root->llink != NULL)
		tdestroy_recurse(root->llink, free_node);
	if (root->rlink != NULL)
		tdestroy_recurse(root->rlink, free_node);
	(*free_node)(root->key);
	free(root);
}

void tdestroy(const posix_tnode *vroot, void (*free_node)(void *nodep))
{
	if (vroot != NULL)
		tdestroy_recurse(vroot, free_node);
}
