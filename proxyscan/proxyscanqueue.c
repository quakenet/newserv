
/*
 * Handle the scan queues
 */

#include "proxyscan.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include <assert.h>

pendingscan *ps_normalqueue=NULL;
pendingscan *ps_prioqueue=NULL;
pendingscan *ps_normalqueueend=NULL;

unsigned int normalqueuedscans=0;
unsigned int prioqueuedscans=0;

unsigned long countpendingscan=0;

void queuescan(patricia_node_t *node, short scantype, unsigned short port, char class, time_t when) {
  pendingscan *psp, *psp2;

  /* we can just blindly queue the scans as node extension cleanhost cache blocks duplicate scans normally.
   * Scans may come from:
   * a) scan <node> (from an oper)
   * b) newnick handler - which ignores clean hosts, only scans new hosts or dirty hosts
   * c) adding a new scan type (rare) 
   */

  /* we should never have an internal node */
  assert(node->prefix);
  /* reference the node - we either start a or queue a single scan */
  patricia_ref_prefix(node->prefix);
  
  /* If there are scans spare, just start it immediately.. 
   * provided we're not supposed to wait */
  if (activescans < maxscans && when<=time(NULL) && ps_ready) {
    startscan(node, scantype, port, class);
    return;
  }

  /* We have to queue it */
  if (!(psp=getpendingscan()))
    Error("proxyscan",ERR_STOP,"Unable to allocate memory");

  countpendingscan++;

  psp->node=node;
  psp->type=scantype;
  psp->port=port;
  psp->class=class;
  psp->when=when;
  psp->next=NULL;

  if (!when) {
    /* normal queue */
    normalqueuedscans++;
    if (ps_normalqueueend) {
      ps_normalqueueend->next=psp;
      ps_normalqueueend=psp;
    } else {
      ps_normalqueueend=ps_normalqueue=psp;
    }
  } else {
    prioqueuedscans++;
    if (!ps_prioqueue || ps_prioqueue->when > when) {
      psp->next=ps_prioqueue;
      ps_prioqueue=psp;
    } else {
      for (psp2=ps_prioqueue; ;psp2=psp2->next) {
	if (!psp2->next || psp2->next->when > when) {
	  psp->next = psp2->next;
	  psp2->next = psp;
	  break;
	}
      }
    }
  }
}

void startqueuedscans() {
  pendingscan *psp=NULL;

  while (activescans < maxscans) {
    if (ps_prioqueue && (ps_prioqueue->when <= time(NULL))) {
      psp=ps_prioqueue;
      ps_prioqueue=psp->next;
      prioqueuedscans--;
    } else if (ps_normalqueue) {
      psp=ps_normalqueue;
      ps_normalqueue=psp->next;
      if (!ps_normalqueue)
	ps_normalqueueend=NULL;
      normalqueuedscans--;
    }
    
    if (psp) {
      startscan(psp->node, psp->type, psp->port, psp->class);
      freependingscan(psp);
      countpendingscan--;
      psp=NULL;
    } else {
      break;
    }
  }
}
    
    
