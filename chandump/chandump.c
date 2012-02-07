#include <stdio.h>
#include <string.h>

#include "../core/schedule.h"
#include "../channel/channel.h"
#include "../lib/version.h"

MODULE_VERSION("");

void *dumpsched;

void dodump(void *arg) {
  chanindex *c;
  int i;
  nick *n;
  char buf[512];

  FILE *fp = fopen("chandump/chandump.txt.1", "w");
  if(!fp)
    return;

  for(i=0;i<CHANNELHASHSIZE;i++)
    for(c=chantable[i];c;c=c->next)
      if(c->channel && !IsSecret(c->channel))
        fprintf(fp, "C %s %d%s%s\n", c->name->content, c->channel->users->totalusers, (c->channel->topic&&c->channel->topic->content)?" ":"", (c->channel->topic&&c->channel->topic->content)?c->channel->topic->content:"");

  for(i=0;i<NICKHASHSIZE;i++)
    for(n=nicktable[i];n;n=n->next)
      fprintf(fp, "N %s %s %s %s %s\n", n->nick, n->ident, strchr(visibleuserhost(n, buf), '@') + 1, (IsAccount(n) && n->authname) ? n->authname : "0", n->realname->name->content);

  fclose(fp);

  rename("chandump/chandump.txt.1", "chandump/chandump.txt");
} 

void _init() {
  dumpsched = (void *)schedulerecurring(time(NULL), 0, 60, &dodump, NULL);
}

void _fini() {
  deleteschedule(dumpsched, &dodump, NULL);
}
