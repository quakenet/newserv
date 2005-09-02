
/*
 * Handle the scan queues
 */

#include "proxyscan.h"

pendingscan *ps_normalqueue=NULL;
pendingscan *ps_prioqueue=NULL;
pendingscan *ps_normalqueueend=NULL;

unsigned int normalqueuedscans=0;
unsigned int prioqueuedscans=0;

void queuescan(unsigned int IP, short scantype, unsigned short port, char class, time_t when) {
  pendingscan *psp, *psp2;
  
  /* If there are scans spare, just start it immediately.. 
   * provided we're not supposed to wait */
  if (activescans < maxscans && when<=time(NULL)) {
    startscan(IP, scantype, port, class);
    return;
  }

  /* We have to queue it */
  psp=getpendingscan();
  psp->IP=IP;
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
    if (ps_prioqueue && ps_prioqueue->when <= time(NULL)) {
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
      startscan(psp->IP, psp->type, psp->port, psp->class);
      freependingscan(psp);
      psp=NULL;
    } else {
      break;
    }
  }
}
    
    
