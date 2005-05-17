/*
  nterfaced end to end transport code
  Copyright (C) 2003-2004 Chris Porter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/sstring.h"
#include "transports.h"
#include "library.h"
#include "logging.h"

struct transport *transports = NULL;

struct transport *register_transport(char *name) {
  struct transport *nt;
  int i;

  sstring *sname = getsstring(name, strlen(name));
  MemCheckR(sname, NULL);

  nt = malloc(sizeof(struct transport));
  if(!nt) {
    freesstring(sname);
    MemError();
    return NULL;
  }

  nt->next = transports;
  transports = nt;

  nt->name = sname;

  for(i=0;i<service_count;i++)
    if(!services[i].transport && !strcmp(services[i].transport_name->content, name))
      services[i].transport = nt;

  return nt;
}

void transport_disconnect(struct transport *tp, int tag) {
  struct request_transport *a, *b;
  struct request *rp, *lp = NULL;

  for(rp=requests;rp;) {
    if(tp->type == TT_INPUT) {
      a = &rp->input;
      b = &rp->output;
    } else {
      a = &rp->output;
      b = &rp->input;
    }

    if((a->transport == tp) && (a->tag == tag)) {
      if(b->transport->on_disconnect)
        b->transport->on_disconnect(rp);
      if(lp) {
        lp->next = rp->next;
        rp->next = NULL;
        free_request(rp);
        rp = lp->next;
      } else {
        rp = rp->next;
        requests->next = NULL;
        free_request(requests);
        requests = rp;
      }
    } else {
      lp = rp;
      rp = rp->next;
    }
  }
}

void deregister_transport(struct transport *tp) {
  int i;
  struct request *rp, *lp = NULL;
  struct transport *ltp = NULL, *cp = transports;
  struct request_transport *a;

  for(rp=requests;rp;) {
    if(rp->output.transport == tp) {
      a = &rp->input;
    } else if(lp->input.transport == tp) {
      a = &rp->output;
    } else {
      lp = rp;
      rp = rp->next;
      continue;
    }

    if(lp) {
      lp->next = rp->next;
    } else {
      requests = rp->next;
    }

    if(tp->on_disconnect)
      tp->on_disconnect(rp);
    if(a->transport->on_disconnect)
      a->transport->on_disconnect(rp);
    free_request(rp);
    if(lp) {
      rp = lp->next;
    } else {
      rp = requests;
    }
  }

  freesstring(tp->name);
  
  for(i=0;i<service_count;i++)
    if(services[i].transport == tp)
      services[i].transport = NULL;

  for(;cp;ltp=cp,cp=cp->next) {
    if(cp == tp) {
      if(ltp) {
        ltp->next = tp->next;
      } else {
        transports = tp->next;
      }
      break;
    }
  }

  free(tp);
}
