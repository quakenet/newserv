
#include <stdlib.h>
#include "patricia.h"
#include <assert.h>
#include "../core/nsmalloc.h"

prefix_t *newprefix() {
  return nsmalloc(POOL_PATRICIA, sizeof(prefix_t));
}

void freeprefix(prefix_t *prefix) {
  nsfree(POOL_PATRICIA, prefix);
}

patricia_node_t *newnode() {
  return nsmalloc(POOL_PATRICIA, sizeof(patricia_node_t));
}

void freenode(patricia_node_t *node) {
  nsfree(POOL_PATRICIA, node);
}


