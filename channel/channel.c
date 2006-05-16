#/* channel.c */

#include "channel.h"
#include "../server/server.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../parser/parser.h"
#include "../irc/irc.h"
#include "../lib/base64.h"
#include "../lib/version.h"

#include <stdio.h>
#include <string.h>

MODULE_VERSION("$Id: channel.c 663 2006-05-16 17:27:36Z newserv $")

#define channelhash(x)  (crc32i(x)%CHANNELHASHSIZE)

unsigned long nouser;
chanindex *chantable[CHANNELHASHSIZE];

const flag cmodeflags[] = {
   { 'n', CHANMODE_NOEXTMSG    },
   { 't', CHANMODE_TOPICLIMIT  },
   { 's', CHANMODE_SECRET      },
   { 'p', CHANMODE_PRIVATE     },
   { 'i', CHANMODE_INVITEONLY  },
   { 'l', CHANMODE_LIMIT       },
   { 'k', CHANMODE_KEY         },
   { 'm', CHANMODE_MODERATE    },
   { 'c', CHANMODE_NOCOLOUR    },
   { 'C', CHANMODE_NOCTCP      },
   { 'r', CHANMODE_REGONLY     },
   { 'D', CHANMODE_DELJOINS    },
   { 'u', CHANMODE_NOQUITMSG   },
   { 'N', CHANMODE_NONOTICE    },
   { '\0', 0 } };   

void channelstats(int hooknum, void *arg);
void sendchanburst(int hooknum, void *arg);

void _init() {
  /* Initialise internal structures */
  initchannelalloc();
  initchannelindex();
  memset(chantable,0,sizeof(chantable));

  /* Set up the nouser marker according to our own numeric */
  nouser=(mylongnum<<18)|CU_NOUSERMASK;
  
  /* Set up our hooks */
  registerhook(HOOK_NICK_NEWNICK,&addordelnick);
  registerhook(HOOK_NICK_LOSTNICK,&addordelnick);
  registerhook(HOOK_CORE_STATSREQUEST,&channelstats);
  registerhook(HOOK_IRC_SENDBURSTBURSTS,&sendchanburst);
  registerhook(HOOK_NICK_WHOISCHANNELS,&handlewhoischannels);
  
  registerserverhandler("B",&handleburstmsg,7);
  registerserverhandler("J",&handlejoinmsg,2);
  registerserverhandler("C",&handlecreatemsg,2);
  registerserverhandler("L",&handlepartmsg,1);
  registerserverhandler("K",&handlekickmsg,3);
  registerserverhandler("T",&handletopicmsg,3);
  registerserverhandler("M",&handlemodemsg,8);
  registerserverhandler("OM",&handlemodemsg,8); /* Treat OPMODE just like MODE */
  registerserverhandler("CM",&handleclearmodemsg,2);
  
  /* If we're connected to IRC, force a disconnect */
  if (connected) {
    irc_send("%s SQ %s 0 :Resync [adding channel support]",mynumeric->content,myserver->content);
    irc_disconnected();  
  }
}

void _fini() {
  deregisterserverhandler("B",&handleburstmsg);
  deregisterserverhandler("J",&handlejoinmsg);
  deregisterserverhandler("C",&handlecreatemsg);
  deregisterserverhandler("L",&handlepartmsg);
  deregisterserverhandler("K",&handlekickmsg);
  deregisterserverhandler("T",&handletopicmsg);
  deregisterserverhandler("M",&handlemodemsg);
  deregisterserverhandler("OM",&handlemodemsg);
  deregisterserverhandler("CM",&handleclearmodemsg);
  
  deregisterhook(HOOK_NICK_NEWNICK,&addordelnick);
  deregisterhook(HOOK_NICK_LOSTNICK,&addordelnick);
  deregisterhook(HOOK_NICK_WHOISCHANNELS,&handlewhoischannels);
}

int addnicktochannel(channel *cp, long numeric) {
  nick *np;
  int i;
  channel **ch;
  void *args[2];
  
  /* Add the channel to the user first, since it might fail */
  if ((np=getnickbynumeric(numeric&CU_NUMERICMASK))==NULL) {
    Error("channel",ERR_ERROR,"Non-existent numeric %ld joined channel %s",numeric,cp->index->name->content);
    return 1;
  }
  
  if (getnumerichandlefromchanhash(cp->users,numeric)) {
    Error("channel",ERR_ERROR,"User %s joined channel %s it was already on!",np->nick,cp->index->name->content);
    return 1;
  }
  
  i=array_getfreeslot(np->channels);
  ch=(channel **)(np->channels->content);
  ch[i]=cp;

  /* Add the user to the channel.
   * I don't expect this while loop to go round many times
   * in the majority of cases */
  while (addnumerictochanuserhash(cp->users,numeric)) {
    rehashchannel(cp);
  }
  
  /* Trigger the hook */
  args[0]=(void *)cp;
  args[1]=(void *)np;
  triggerhook(HOOK_CHANNEL_NEWNICK,args);
  
  return 0;
}

void delnickfromchannel(channel *cp, long numeric, int updateuser) {
  int i;
  channel **ch;
  nick *np;
  unsigned long *lp;
  int found=0;
  void *args[2];
  
  if ((np=getnickbynumeric(numeric&CU_NUMERICMASK))==NULL) {
    Error("channel",ERR_ERROR,"Trying to remove non-existent nick %d from channel %s",numeric,cp->index->name);
    return;
  }
  
  args[0]=(void *)cp;
  args[1]=(void *)np;
  
  if ((lp=getnumerichandlefromchanhash(cp->users,numeric))==NULL) {
    /* User wasn't on the channel.  It's perfectly legit that this can happen. */
    return;
  } else {
    triggerhook(HOOK_CHANNEL_LOSTNICK,args);
    *lp=nouser;
    if (--cp->users->totalusers==0) {
      /* We're deleting the channel; flag it here */
      triggerhook(HOOK_CHANNEL_LOSTCHANNEL,cp);
      delchannel(cp);
    }    
  }

  /* The updateuser part is optional; if the user is parting _all_ their channels
   * at once we don't bother hunting each channel down we just blat the array */
  
  if (updateuser) {
    ch=(channel **)(np->channels->content);
    for (i=0;i<np->channels->cursi;i++) {
      if (ch[i]==cp) {
        array_delslot(np->channels,i);
        found=1;
        break;
      }
    }
    if (found==0) {
      Error("channel",ERR_ERROR,"Trying to remove nick %s from channel %s it was not on (chan not on nick)",np->nick,cp->index->name->content);
    }
  }  
}

void delchannel(channel *cp) {
  chanindex *cip;
  
  /* Remove entry from index */
  cip=cp->index;
  cip->channel=NULL;
  releasechanindex(cip);
  
  freesstring(cp->topic);
  freesstring(cp->key);
  freechanuserhash(cp->users);
  clearallbans(cp);
  freechan(cp);
}

channel *createchannel(char *name) {
  channel *cp;
  chanindex *cip;
  sstring *ssp;
  
  cip=findorcreatechanindex(name);
  
  if (cip->channel) {
    Error("channel",ERR_ERROR,"Attempting to create existing channel %s (%s).",cip->name->content,name);
    return cip->channel;
  }
  
  /* Check that chanindex record has the same capitalisation as actual channel */
  if (strcmp(cip->name->content,name)) { 
    ssp=cip->name;
    cip->name=getsstring(name,CHANNELLEN);
    freesstring(ssp);
  }
  
  cp=newchan();
  cip->channel=cp;
  cp->index=cip;
  
  cp->timestamp=MAGIC_REMOTE_JOIN_TS;
  cp->topic=NULL;
  cp->topictime=0;
  cp->flags=0;
  cp->key=NULL;
  cp->limit=0;
  cp->bans=NULL;
  cp->users=newchanuserhash(1);
  
  return cp;
}
 
channel *findchannel(char *name) {
  chanindex *cip;
  
  cip=findchanindex(name);
  if (cip==NULL) {
    return NULL;
  } else {
    return cip->channel;
  }
}

void channelstats(int hooknum, void *arg) {
  int level=(int)arg;
  int i,curchain,maxchain=0,total=0,buckets=0,realchans=0;
  int users=0,slots=0;
  chanindex *cip;
  char buf[100];
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    if (chantable[i]!=NULL) {
      buckets++;
      curchain=0;
      for (cip=chantable[i];cip;cip=cip->next) {
        total++;
        curchain++;
        if (cip->channel!=NULL) {
          realchans++;
          users+=cip->channel->users->totalusers;
          slots+=cip->channel->users->hashsize;
        }
      } 
      if (curchain>maxchain) {
        maxchain=curchain;
      }
    }
  }

  if (level>5) {
    /* Full stats */
    sprintf(buf,"Channel : %6d channels (HASH: %6d/%6d, chain %3d)",total,buckets,CHANNELHASHSIZE,maxchain);
    triggerhook(HOOK_CORE_STATSREPLY,buf);
    
    sprintf(buf,"Channel :%7d channel users, %7d slots allocated, efficiency %.1f%%",users,slots,(float)(100*users)/slots);
    triggerhook(HOOK_CORE_STATSREPLY,buf);
  } 
  
  if (level>2) {
    sprintf(buf,"Channel : %6d channels formed.",realchans);
    triggerhook(HOOK_CORE_STATSREPLY,buf);
  }
} 


/*
 * Deal with users joining the network (create and initialise their channel array)
 * or leaving the network (remove them from all channels and delete the array)
 */

void addordelnick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  channel **ch;
  int i;
  
  switch(hooknum) {
    case HOOK_NICK_NEWNICK:
      np->channels=(array *)malloc(sizeof(array));
      array_init(np->channels,sizeof(channel **));
      array_setlim1(np->channels,10);
      array_setlim2(np->channels,15);
      break;
      
    case HOOK_NICK_LOSTNICK:
      ch=(channel **)(np->channels->content);
      for(i=0;i<np->channels->cursi;i++) {
        delnickfromchannel(ch[i],np->numeric,0);
      }
      array_free(np->channels);
      free(np->channels);
      break;
  }
}

/*
 * Spam our local burst on connect..
 */
 
void sendchanburst(int hooknum, void *arg) {
  chanindex *cip;
  channel *cp;
  chanban *ban;
  char buf[BUFSIZE];
  char buf2[20];
  char *banstr;
  int i;
  int j,k;
  int bufpos;
  int newmode=1;
  int newline=1;
  long modeorder[] = { 0, CUMODE_OP, CUMODE_VOICE, CUMODE_OP|CUMODE_VOICE };
  long curmode;
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      cp=cip->channel;
      if (cp==NULL) {
        continue;
      }
      /* Set up the burst */
      sprintf(buf2,"%d ",cp->limit);
      bufpos=sprintf(buf,"%s B %s %lu %s %s%s%s",mynumeric->content,cip->name->content,cp->timestamp,
        printflags(cp->flags,cmodeflags),IsLimit(cp)?buf2:"",
        IsKey(cp)?cp->key->content:"",IsKey(cp)?" ":"");
      for(j=0;j<4;j++) {
        curmode=modeorder[j];
        newmode=1;
        for (k=0;k<cp->users->hashsize;k++) {
          if (cp->users->content[k]!=nouser && ((cp->users->content[k]&(CU_MODEMASK))==curmode)) {
            /* We found a user of the correct type for this pass */
            if (BUFSIZE-bufpos<10) { /* Out of space.. wrap up the old line and send a new one */
              newmode=newline=1;
              irc_send("%s",buf);
              bufpos=sprintf(buf,"%s B %s %lu ",mynumeric->content,cip->name->content,cp->timestamp);
            }
            if (newmode) {
              bufpos+=sprintf(buf+bufpos,"%s%s%s%s%s",newline?"":",",longtonumeric(cp->users->content[k]&CU_NUMERICMASK,5),
                (curmode==0?"":":"),(curmode&CUMODE_OP)?"o":"",(curmode&CUMODE_VOICE)?"v":"");
            } else {
              bufpos+=sprintf(buf+bufpos,",%s",longtonumeric(cp->users->content[k]&CU_NUMERICMASK,5));
            }
            newmode=newline=0;
          } /* if(...) */
        } /* for (k=..) */
      } /* for (j=..) */
      
      /* And now the bans */
      newline=1;
      for(ban=cp->bans;ban;ban=ban->next) {
	banstr=bantostring(ban);
        if ((BUFSIZE-bufpos)<(strlen(banstr)+10)) { /* Out of space.. wrap up the old line and send a new one */
          newline=1;
          irc_send("%s",buf);
          bufpos=sprintf(buf,"%s B %s %lu",mynumeric->content,cip->name->content,cp->timestamp);
        }
        bufpos+=sprintf(buf+bufpos,"%s%s ",(newline?" :%":""),banstr);
        newline=0;
      }
      irc_send("%s",buf);
    } /* for(cp..) */
  } /* for (i..) */ 
}

/*
 * countuniquehosts:
 *  Uses the marker on all host records to count unique hosts
 *  on a channel in O(n) time (n is channel user hash size).
 */
  
unsigned int countuniquehosts(channel *cp) {
  unsigned int marker;
  int i;
  int count=0;
  nick *np;
  
  marker=nexthostmarker();
  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;
      
    if ((np=getnickbynumeric(cp->users->content[i]))==NULL) {
      Error("channel",ERR_ERROR,"Found unknown numeric %u on channel %s",cp->users->content[i],cp->index->name->content);
      continue;
    }
    
    if (np->host->marker==marker)
      continue;

    np->host->marker=marker;
    count++;
  }
  
  return count;
}
