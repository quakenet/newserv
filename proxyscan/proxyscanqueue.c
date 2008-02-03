
/*
 * Handle the scan queues
 */

#include "proxyscan.h"
#include "../irc/irc.h"
#include "../core/error.h"

pendingscan *ps_normalqueue=NULL;
pendingscan *ps_prioqueue=NULL;
pendingscan *ps_normalqueueend=NULL;

unsigned int normalqueuedscans=0;
unsigned int prioqueuedscans=0;

unsigned long countpendingscan=0;

void queuescan(patricia_node_t *node, short scantype, unsigned short port, char class, time_t when) {
  pendingscan *psp, *psp2;

  /* check if the IP/port combo is already queued - don't queue up
   * multiple identical scans
   */
  psp = ps_prioqueue;
  while (psp != NULL)
  {
    if (psp->node == node && psp->type == scantype &&
      psp->port == port && psp->class == class)
    {
      /* found it, ignore */
      return;
    }
    psp = psp->next;
  }

  psp = ps_normalqueue;
  while (psp != NULL)
  {
    if (psp->node == node && psp->type == scantype &&
      psp->port == port && psp->class == class)
    {
      /* found it, ignore */
      return;
    }
    psp = psp->next;
  }
  
  /* If there are scans spare, just start it immediately.. 
   * provided we're not supposed to wait */
  if (activescans < maxscans && when<=time(NULL) && (ps_start_ts+120 <= time(NULL))) {
    startscan(node, scantype, port, class);
    return;
  }

  /* We have to queue it */
  if (!(psp=(struct pendingscan *)malloc(sizeof(pendingscan))))
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

  if (ps_start_ts+120 > time(NULL))
	return;

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
      free(psp);
      countpendingscan--;
      psp=NULL;
    } else {
      break;
    }
  }
}
    
    
