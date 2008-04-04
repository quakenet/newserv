
#include <stdlib.h>
#include "patricia.h"
#include <assert.h>
#include "../core/nsmalloc.h"

#define ALLOCUNIT  100

patricia_node_t *node_freelist;
union prefixes *prefix_freelist;

prefix_t *newprefix() {
    union prefixes *prefixes = prefix_freelist;
    int i;

    if (prefixes==NULL) {
      prefixes=(union prefixes *)nsmalloc(POOL_PATRICIA,ALLOCUNIT*sizeof(prefix_t));

      for (i=0;i<(ALLOCUNIT-1);i++) {
        prefixes[i].next=&(prefixes[i+1]);
      }
      prefixes[ALLOCUNIT-1].next=NULL;
    }

    prefix_freelist = prefixes->next;
    return &(prefixes->prefix);

}

void freeprefix (prefix_t *prefix) {
  union prefixes *ups =  (union prefixes *)prefix;
  ups->next = prefix_freelist;
  prefix_freelist = ups;
}


patricia_node_t *newnode() {
  int i;
  patricia_node_t *node;

  if( node_freelist==NULL ) {
    node_freelist=(patricia_node_t *)nsmalloc(POOL_PATRICIA,ALLOCUNIT*sizeof(patricia_node_t));

    for (i=0;i<(ALLOCUNIT-1);i++) {
      node_freelist[i].parent=(patricia_node_t *)&(node_freelist[i+1]);
    }
    node_freelist[ALLOCUNIT-1].parent=NULL;
  }

  node=node_freelist;
  node_freelist=(patricia_node_t *)node->parent;

  return node;
}

void freenode (patricia_node_t *node) {
 node->parent=(patricia_node_t *)node_freelist;
 node_freelist=node;
}


