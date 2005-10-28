/* lameisp.c */

#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"

#define LI_CLONEMAX 2
#define LI_KILL_MESSAGE "excess clones from your host"

static char *li_isps[] = {"*.t-dialin.net", NULL};

void li_nick(int hooknum, void *arg);
void li_killyoungest(nick *np);
int li_stats(void *source, int cargc, char **cargv);

int li_victims = 0;
int li_ispscount;

void _init() {
  host *hp;
  int i, j;
  
  /* little optimisation */
  for(i=0;li_isps[i];i++);
  li_ispscount = i;
  
  /* ok, first time loading we have to go through every host
     and check for excess clones, and obviously kill the excess */
     
  for (j=0;j<HOSTHASHSIZE;j++)
    for(hp=hosttable[j];hp;hp=hp->next)
      if (hp->clonecount > LI_CLONEMAX) /* if we have too many clones */
        for(i=0;i<li_ispscount;) /* cycle through the list of isps */
          if (match2strings(li_isps[i++], hp->name->content)) /* if our isp matches */
            do { /* repeatedly kill the youngest until we're within acceptable boundaries */
              li_killyoungest(hp->nicks);
            } while (hp->clonecount > LI_CLONEMAX);
            
  registercontrolhelpcmd("victims", NO_OPER, 0, li_stats, "Usage: victims\nShows the amount of clients victimised by lameisp.");
  registerhook(HOOK_NICK_NEWNICK, &li_nick);
}

void _fini () {
  deregisterhook(HOOK_NICK_NEWNICK, &li_nick);
  deregistercontrolcmd("victims", li_stats);
}

void li_nick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  int i;
  if (np->host->clonecount > LI_CLONEMAX)
    for(i=0;i<li_ispscount;) /* cycle through isps */
      if (match2strings(li_isps[i++], np->host->name->content))
        li_killyoungest(np->host->nicks); /* kill only youngest as we have exactly LI_CLONEMAX clones */
}

void li_killyoungest(nick *np) {
  nick *list = np, *youngest = list;
  for(list=list->host->nicks;list->next;list=list->nextbyhost) /* cycle through all nicks with this host */
    if (list->timestamp < youngest->timestamp) /* if we've got the youngest here */
      youngest = list; /* set value to this one */
  
  li_victims++; /* ah, fresh victims for my ever-growing army of the undead */
  killuser(NULL, youngest, LI_KILL_MESSAGE); /* byebye */
}

int li_stats(void *source, int cargc, char **cargv) {
  controlreply((nick *)source, "Victims: %d clients", li_victims);
  
  return CMD_OK;
}
