#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "../control/control.h"
#include "glines.h"

static void statusfn(int, void *);
MODULE_VERSION("");

void _init() {
  /* If we're connected to IRC, force a disconnect. */
  if (connected) {
    irc_send("%s SQ %s 0 :Resync [adding jupe support]",mynumeric->content,myserver->content); irc_disconnected();
  }

  registerserverhandler("GL", &handleglinemsg, 6);
  registerhook(HOOK_CORE_STATSREQUEST, statusfn);

}

void _fini() {
  deregisterserverhandler("GL", &handleglinemsg);
  deregisterhook(HOOK_CORE_STATSREQUEST, statusfn);
}

static int countmatchingusers(const char *identmask, struct irc_in_addr *ip, unsigned char bits) {
  nick *np;
  int i;
  int count = 0;

  for(i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=np->next)
      if (ipmask_check(&np->p_nodeaddr, ip, bits) && match2strings(identmask, np->ident))
        count++;

  return count;
}

void glinesetmask(const char *mask, int duration, const char *reason, const char *creator) {
  gline *g;
  time_t t = getnettime();
  sstring *screator;

  if(strcmp(creator, "trusts_policy")==0) {
    controlwall(NO_OPER, NL_GLINES, "(NOT) Setting gline on '%s' lasting %s with reason '%s', created by: %s", mask, longtoduration(duration, 0), reason, creator);
    return;
  }

  if (!(g = gline_find(mask))) {
    // new gline
    screator = getsstring(creator, 255);
    g = gline_add(screator, mask, reason, t+duration, t, t+duration);
    gline_propagate(g);
  } else {
    // existing gline
    // in 1.4, can update expire, reason etc
    // in 1.3 can only activate an existing inactive gline
    if( !(g->flags & GLINE_ACTIVE) )  {
      gline_activate(g,0,1);
    }
  }
}

void glineremovemask(const char *mask) {
  controlwall(NO_OPER, NL_GLINES, "Removing gline on '%s'.", mask);
  irc_send("%s GL * -%s", mynumeric->content, mask);
}

static void glinesetmask_cb(const char *mask, int hits, void *arg) {
  gline_params *gp = arg;
  glinesetmask(mask, gp->duration, gp->reason, gp->creator);
}

int glinesuggestbyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int flags, gline_callback callback, void *uarg) {
  trusthost *th, *oth;
  int count;
  unsigned char nodebits;

  nodebits = getnodebits(ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(ip);

    if(th && (th->group->flags & TRUST_ENFORCE_IDENT)) { /* Trust with enforceident enabled */
      count = 0;

      for(oth=th->group->hosts;oth;oth=oth->next)
        count += glinesuggestbyip(user, &oth->ip, oth->bits, flags | GLINE_ALWAYS_USER | GLINE_IGNORE_TRUST, callback, uarg);

      return count;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    user = "*";

  /* Widen gline to match the node mask. */
  if(nodebits<bits)
    bits = nodebits;

  count = countmatchingusers(user, ip, bits);

  if (!(flags & GLINE_SIMULATE)) {
    char mask[512];
    snprintf(mask, sizeof(mask), "%s@%s", user, trusts_cidr2str(ip, bits));
    callback(mask, count, uarg);
  }

  return count;
}

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator) {
  gline_params gp;

  if(!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(user, ip, bits, flags | GLINE_SIMULATE, NULL, NULL);
    if(hits>MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested gline(s) for '%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
        user, trusts_cidr2str(ip, bits), longtoduration(duration, 0), reason, hits, MAXGLINEUSERS);
      return 0;
    }
  }

  gp.duration = duration;
  gp.reason = reason;
  gp.creator = creator;
  return glinesuggestbyip(user, ip, bits, flags, glinesetmask_cb, &gp);
}

int glinesuggestbynick(nick *np, int flags, void (*callback)(const char *, int, void *), void *uarg) {
  if (flags & GLINE_ALWAYS_NICK) {
    if(!(flags & GLINE_SIMULATE)) {
      char mask[512];
      snprintf(mask, sizeof(mask), "%s!*@*", np->nick);
      callback(mask, 1, uarg);
    }

    return 1;
  } else {
    return glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags, callback, uarg);
  }
}

int glinebynick(nick *np, int duration, const char *reason, int flags, const char *creator) {
  gline_params gp;

  if(!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags | GLINE_SIMULATE, NULL, NULL);
    if(hits>MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested gline(s) for nick '%s!%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
        np->nick, np->ident, IPtostr(np->p_ipaddr), longtoduration(duration, 0), reason, hits, MAXGLINEUSERS);
      return 0;
    }
  }

  gp.duration = duration;
  gp.reason = reason;
  gp.creator = creator;
  return glinesuggestbynick(np, flags, glinesetmask_cb, &gp);
}

gline *gline_add(sstring *creator, char *mask, char *reason, time_t expires, time_t lastmod, time_t lifetime) {
  gline *gl;

  if ( !(gl=makegline(mask))) { /* sets up nick,user,host,node and flags */
    /* couldn't process gline mask */
    Error("gline", ERR_WARNING, "Tried to add malformed G-Line %s!", mask);
    return 0;
  }

  gl->creator = creator;

  /* it's not unreasonable to assume gline is active, if we're adding a deactivated gline, we can remove this later */
  gl->flags |= GLINE_ACTIVE;

  gl->reason = getsstring(reason, 255);
  gl->expires = expires;
  gl->lastmod = lastmod;
  gl->lifetime = lifetime;

  gl->next = glinelist;
  glinelist = gl;

  return gl;
}

gline *makegline(char *mask) {
  /* populate gl-> user,host,node,nick and set appropriate flags */
  int len;
  int foundat=-1,foundbang=-1;
  int foundwild=0;
  int i;
  struct irc_in_addr sin;
  unsigned char bits;
  gline *gl = NULL;

  Error("gline", ERR_FATAL,"processing: %s", mask);
  if (!(gl = getgline())) {
    Error("gline", ERR_ERROR, "Failed to allocate new gline");
    return 0;
  }

  len=strlen(mask);

  switch (*mask ) {
    case '#':
    case '&':
      gl->flags |= GLINE_BADCHAN;
      gl->user = getsstring(mask, CHANNELLEN);
      return gl;
   case '$':
      switch (mask[1]) {
        case 'R':
          gl->flags |= GLINE_REALNAME;
          break;
        default:
          return 0;
      }
      gl->user = getsstring(mask,REALLEN);
      return gl;
   default:
      /* Default case of some host/ip/cidr mask */
      for (i=(len-1);i>=0;i--) {
        if (mask[i]=='@') {
          /* host */
          if ((len-i)-1 > HOSTLEN) {
            /* host too long */
            gl->flags |= GLINE_BADMASK;
          } else if (i==(len-1)) {
            /* no host supplied aka gline ends @ */
            gl->flags |= GLINE_BADMASK;
          } else if (i==(len-2) && mask[i+1]=='*') {
            /* Special case: "@*" */
            gl->flags |= GLINE_HOSTANY;
          } else {
            if (ipmask_parse(&mask[i+1], &sin, &bits) == 0) {
              /* we have some host string */
              gl->host=getsstring(&mask[i+1],HOSTLEN);
              if (foundwild) {
                gl->flags |= GLINE_HOSTMASK;
              } else {
                gl->flags |= GLINE_HOSTEXACT;
             }
            } else {
              /* we have a / so cidr gline */
              gl->ip = sin;
              gl->bits = bits;
              gl->host=getsstring(&mask[i+1],HOSTLEN);
              if (!is_normalized_ipmask(&sin, bits)) {
                gl->flags |= GLINE_BADMASK;
              }
              gl->flags |= GLINE_IPMASK;
            }
          }
          foundat=i;
          break;
        } else if (mask[i]=='?' || mask[i]=='*') {
          if (!foundwild)  /* Mark last wildcard in string */
            foundwild=i;
        }
      }
  }

  if (foundat<0) {
    /* If there wasn't an @, this ban matches any host */
    gl->host=NULL;
    gl->flags |= GLINE_HOSTANY;
  }

  foundwild=0;

  for (i=0;i<foundat;i++) {
    if (mask[i]=='!') {
      if (i==0) {
        /* Invalid mask: nick is empty */
        gl->flags |= GLINE_NICKNULL;
        gl->nick=NULL;
      } else if (i==1 && mask[0]=='*') {
        /* matches any nick */
        gl->flags |= GLINE_NICKANY;
        gl->nick=NULL;
      } else {
        if (i>NICKLEN) {
          /* too long */
		  gl->flags |= GLINE_BADMASK;
        }
        gl->nick=getsstring(mask,i);
        if (foundwild)
          gl->flags |= GLINE_NICKMASK;
        else
          gl->flags |= GLINE_NICKEXACT;
      }
      foundbang=i;
      break;
    } else if (mask[i]=='?' || mask[i]=='*') {
      if (i<NICKLEN) {
        foundwild=1;
      }
    }
  }

  if (foundbang<0) {
    /* We didn't find a !, what we do now depends on what happened
     * with the @ */
    if (foundat<0) {
      /* A gline with no ! or @ is treated as a host gline only */
      /* Note that we've special-cased "*" at the top, so we can only
       * hit the MASK or EXACT case here. */
      gl->host=getsstring(mask,len);

      if (foundwild)
        gl->flags |= GLINE_HOSTMASK;
      else
        gl->flags |= GLINE_HOSTEXACT;

      gl->flags |= (GLINE_USERANY | GLINE_NICKANY);
      gl->nick=NULL;
      gl->user=NULL;
    } else {
      /* A gline with @ only is treated as user@host */
      gl->nick=NULL;
      gl->flags |= GLINE_NICKANY;
    }
  }

  if (foundat>=0) {
    /* We found an @, so everything between foundbang+1 and foundat-1 is supposed to be ident */
    /* This is true even if there was no !.. */
    if (foundat==(foundbang+1)) {
      /* empty ident matches nothing */ /*@@@TODO: * for glines? */
      gl->flags |= (GLINE_BADMASK | GLINE_USERNULL);
    } else if (foundat - foundbang - 1 > USERLEN) {
      /* It's too long.. */
	  gl->flags |= GLINE_BADMASK;
    } else if ((foundat - foundbang - 1 == 1) && mask[foundbang+1]=='*') {
      gl->flags |= GLINE_USERANY;
    } else {
      gl->user=getsstring(&mask[foundbang+1],(foundat-foundbang-1));
      if (strchr(gl->user->content,'*') || strchr(gl->user->content,'?'))
        gl->flags |= GLINE_USERMASK;
      else
        gl->flags |= GLINE_USEREXACT;
    }
    /* Username part can't contain an @ */
    if (gl->user && strchr(gl->user->content,'@')) {
      gl->flags |= GLINE_BADMASK;
    }
  }

  assert(gl->flags & (GLINE_USEREXACT | GLINE_USERMASK | GLINE_USERANY | GLINE_USERNULL));
  assert(gl->flags & (GLINE_NICKEXACT | GLINE_NICKMASK | GLINE_NICKANY | GLINE_NICKNULL));
  assert(gl->flags & (GLINE_HOSTEXACT | GLINE_HOSTMASK | GLINE_HOSTANY | GLINE_HOSTNULL | GLINE_IPMASK));

  if (gl->flags & GLINE_BADMASK) {
    Error("gline", ERR_WARNING, "BADMASK: %s", mask);
	return 0;
  }

  // Start: Safety Check for now...
  if ( strcmp( glinetostring(gl), mask ) ) {
    // oper can specify * to glist, ircd by default converts * to *!*@*
    if ( ((gl->flags & ( GLINE_USERANY | GLINE_NICKANY | GLINE_HOSTANY )) == ( GLINE_USERANY | GLINE_NICKANY | GLINE_HOSTANY )) && !strcmp("*",mask)){
      return gl;
    }
    // oper can specifiy *@host, ircd by default converst *@host to *!*@host
    if ( ((gl->flags & ( GLINE_NICKANY )) == ( GLINE_NICKANY )) && (mask[1]!='!') ) {
      Error("gline", ERR_WARNING, "different: %s %s", glinetostring(gl), mask);
      return gl;
    }

    Error("gline", ERR_WARNING, "DIFFERENT: %s %s", glinetostring(gl), mask);
  }
  // End: Safety Check for now...

  return gl;
}

gline *gline_find( char *mask) {
  gline *gl, *sgl;
  gline *globalgline;
  time_t curtime = time(0);

  if( !(globalgline=makegline(mask))) {
    /* gline mask couldn't be processed */
    return 0;
  }

 for (gl = glinelist; gl; gl = sgl) {
    sgl = gl->next;

    if (gl->lifetime <= curtime) {
      removegline(gl);
      continue;
    } else if (gl->expires <= curtime) {
      gl->flags &= ~GLINE_ACTIVE;
    }

    if ( glineequal( globalgline, gl ) ) {
      freegline(globalgline);
      return gl;
    }
  }

  freegline(globalgline);
  return 0;
}

char *glinetostring(gline *g) {
  static char outstring[NICKLEN+USERLEN+HOSTLEN+5]; // check
  int strpos=0;

  if ( g->flags & GLINE_REALNAME ) {
   if (g->user)
     strpos += sprintf(outstring+strpos, "%s", g->user->content);
   // @@ todo $R*
   // add $R (not check badchans need same?)
   return outstring;
  }

  if ( g->flags & GLINE_BADCHAN ) {
    if (g->user)
      strpos += sprintf(outstring+strpos, "%s", g->user->content);
    return outstring;
  }

  if ( g->flags & GLINE_NICKANY ) {
    strpos += sprintf(outstring+strpos,"*");
  } else if (g->nick) {
    strpos += sprintf(outstring+strpos,"%s", g->nick->content);
  }

  strpos += sprintf(outstring+strpos, "!");

  if ( g->flags & GLINE_USERANY ) {
    strpos += sprintf(outstring+strpos, "*");
  } else if (g->user) {
    strpos += sprintf(outstring+strpos, "%s", g->user->content);
  }

  strpos += sprintf(outstring+strpos, "@");

  if ( g->flags & GLINE_HOSTANY ) {
    strpos += sprintf(outstring+strpos, "*");
  } else if (g->host) {
    strpos += sprintf(outstring+strpos, "%s", g->host->content);
  } else if ( g->flags & GLINE_IPMASK ) {
    strpos += sprintf(outstring+strpos, "%s", g->host->content);
  }
  return outstring;
}

gline *gline_activate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();
  agline->flags |= GLINE_ACTIVE;

  if (lastmod) {
    agline->lastmod = lastmod;
  } else {
    if(now<=agline->lastmod)
      agline->lastmod++;
    else
      agline->lastmod=now;
  }

  if ( propagate) {
    gline_propagate(agline);
  }
  return agline;
}

gline *gline_deactivate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();
  agline->flags &= ~GLINE_ACTIVE;

  if (lastmod) {
    agline->lastmod = lastmod;
  } else {
    if(now<=agline->lastmod)
      agline->lastmod++;
    else
      agline->lastmod=now;
  }

  if ( propagate) {
    gline_propagate(agline);
  }
  return agline;
}

void gline_propagate(gline *agline) {
  if ( agline->flags & GLINE_ACTIVE ) {
    controlwall(NO_OPER, NL_GLINES, "Setting gline on '%s' lasting %s with reason '%s', created by: %s", glinetostring(agline), longtoduration(agline->expires-getnettime(),0), agline->reason->content, agline->creator->content);
    irc_send("%s GL * +%s %lu %lu :%s\r\n", mynumeric->content, glinetostring(agline), agline->expires, agline->lastmod, agline->reason->content);
  } else {
    irc_send("%s GL * -%s %lu %lu :%s\r\n", mynumeric->content, glinetostring(agline), agline->expires, agline->lastmod, agline->reason->content);
  }
}

/* returns non-zero if the glines are exactly the same */
int glineequal ( gline *gla, gline *glb) {
  if ((!gla->nick && glb->nick) || (gla->nick && !glb->nick))
    return 0;

  if (gla->nick && ircd_strcmp(gla->nick->content,glb->nick->content))
    return 0;

  if ((!gla->user && glb->user) || (gla->user && !glb->user))
    return 0;

  if (gla->user && ircd_strcmp(gla->user->content,glb->user->content))
    return 0;

  if ((!gla->host && glb->host) || (gla->host && !glb->host))
    return 0;

  if (gla->host && ircd_strcmp(gla->host->content,glb->host->content))
    return 0;
  
  /* TODO @@@ match bits flags */
  return 1;
}

static void statusfn(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];
    int glcount = 0;
    gline *gl;

    for(gl=glinelist;gl;gl=gl->next) {
      glcount++;
    }

    snprintf(message, sizeof(message), "Glines  :%7d glines", glcount);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  } 
}

/* returns non-zero on match */
int gline_match_mask ( gline *gla, gline *glb) {
  if ((!gla->nick && glb->nick) || (gla->nick && !glb->nick))
    return 0;

  if (gla->nick && match(gla->nick->content,glb->nick->content))
    return 0;

  // TODO should check for ANY etc
  if ((!gla->user && glb->user) || (gla->user && !glb->user))
    return 0;

  if (gla->user && match(gla->user->content,glb->user->content))
    return 0;

  if ((!gla->host && glb->host) || (gla->host && !glb->host))
    return 0;

  if (gla->host && match(gla->host->content,glb->host->content))
    return 0;

  /* TODO @@@ match bits flags */
  return 1;
}

