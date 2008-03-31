#include "../core/nsmalloc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "patricia.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../control/control.h"

#include <assert.h> /* assert */
#include <stdio.h>
#include <string.h>

MODULE_VERSION("");

patricia_tree_t *iptree;
sstring *nodeextnames[PATRICIA_MAXSLOTS];

void patriciastats(int hooknum, void *arg);

void _init(void) {
  iptree = patricia_new_tree(PATRICIA_MAXBITS);  
  assert(iptree);

  registerhook(HOOK_CORE_STATSREQUEST,&patriciastats);
}

void _fini(void) {
  deregisterhook(HOOK_CORE_STATSREQUEST,&patriciastats);
  patricia_destroy_tree (iptree, NULL);
  nsfreeall(POOL_PATRICIA);
}

void patriciastats(int hooknum, void *arg) {
  long level=(long)arg;
  char buf[100];
  patricia_node_t *head, *node;
  int i,j,k,l;

  if (level < 5) 
    return; 

  sprintf(buf, "Patricia: %6d Active Nodes (%d bits)", iptree->num_active_node, iptree->maxbits);
  triggerhook(HOOK_CORE_STATSREPLY,buf);

  head = iptree->head;

  i=0;j=0;
  PATRICIA_WALK_ALL(head, node) {
     if ( node->prefix ) 
       j++;
     else 
       i++; 
  } PATRICIA_WALK_END;
  sprintf(buf, "Patricia: %6d Nodes,   %6d Prefix (walk all)", i,j);
  triggerhook(HOOK_CORE_STATSREPLY,buf);

  head = iptree->head;
  i=0;j=0;k=0;l=0;
  PATRICIA_WALK(head, node) {
     if ( node->prefix ) {
       if (irc_in_addr_is_ipv4(&(node->prefix->sin)))
         k++;
       else
         l++;
       j++;
     } else
       i++;
  } PATRICIA_WALK_END;
  sprintf(buf, "Patricia: %6d Nodes,   %6d Prefix (walk prefixes only)", i,j);
  triggerhook(HOOK_CORE_STATSREPLY,buf);
  sprintf(buf, "Patricia: %6d IP4Node, %6d IP6Node", k, l);
  triggerhook(HOOK_CORE_STATSREPLY,buf);

  j=0;
  for (i=0;i<PATRICIA_MAXSLOTS;i++) {
    if (nodeextnames[i]!=NULL) {
      j++;
    }
  }
  sprintf(buf, "Patricia: %6d ExtsUsed, %5d Max", j,PATRICIA_MAXSLOTS);
  triggerhook(HOOK_CORE_STATSREPLY,buf);

}

int registernodeext(const char *name) {
  int i;

  if (findnodeext(name)!=-1) {
    Error("patricia",ERR_WARNING,"Tried to register duplicate node extension %s",name);
    return -1;
  }

  for (i=0;i<PATRICIA_MAXSLOTS;i++) {
    if (nodeextnames[i]==NULL) {
      nodeextnames[i]=getsstring(name,100);
      return i;
    }
  }

  Error("patricia",ERR_WARNING,"Tried to register too many extensions: %s",name);
  return -1;
}

int findnodeext(const char *name) {
  int i;

  for (i=0;i<PATRICIA_MAXSLOTS;i++) {
    if (nodeextnames[i]!=NULL && !ircd_strcmp(name,nodeextnames[i]->content)) {
      return i;
    }
  }

  return -1;
}

void releasenodeext(int index) {
  patricia_node_t *head, *node;

  freesstring(nodeextnames[index]);
  nodeextnames[index]=NULL;

  head = iptree->head;

  PATRICIA_WALK_ALL(head, node)
  {
      node->exts[index]=NULL;
  } PATRICIA_WALK_END;
}

