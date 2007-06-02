/* Functions for manipulating local users on channels */

#include "localuser.h"
#include "localuserchannel.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../irc/irc.h"
#include "../lib/version.h"
#include "../lib/sstring.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

MODULE_VERSION("");

typedef struct pendingkick {
  nick *source, *target;
  channel *chan;
  sstring *reason;
  struct pendingkick *next;
} pendingkick;

pendingkick *pendingkicklist;

int handlechannelmsgcmd(void *source, int cargc, char **cargv);
int handlechannelnoticecmd(void *source, int cargc, char **cargv);
int handleinvitecmd(void *source, int cargc, char **cargv);
void clearpendingkicks(int hooknum, void *arg);
void checkpendingkicknicks(int hooknum, void *arg);
void checkpendingkickchannels(int hooknum, void *arg);
void _localkickuser(nick *np, channel *cp, nick *target, const char *message);
void luc_handlekick(int hooknum, void *arg);

void _init() {
  pendingkicklist=NULL;
  registerserverhandler("P",&handlechannelmsgcmd,2);
  registerserverhandler("O",&handlechannelnoticecmd,2);
  registerserverhandler("I",&handleinvitecmd,2);
  registerhook(HOOK_CHANNEL_KICK, luc_handlekick);
  registerhook(HOOK_NICK_LOSTNICK, checkpendingkicknicks);
  registerhook(HOOK_CHANNEL_LOSTCHANNEL, checkpendingkickchannels);
  registerhook(HOOK_CORE_ENDOFHOOKSQUEUE,&clearpendingkicks);
}

void _fini() {
  pendingkick *pk;

  deregisterserverhandler("P",&handlechannelmsgcmd);
  deregisterserverhandler("O",&handlechannelnoticecmd);
  deregisterserverhandler("I",&handleinvitecmd);
  deregisterhook(HOOK_CHANNEL_KICK, luc_handlekick);
  deregisterhook(HOOK_NICK_LOSTNICK, checkpendingkicknicks);
  deregisterhook(HOOK_CHANNEL_LOSTCHANNEL, checkpendingkickchannels);
  deregisterhook(HOOK_CORE_ENDOFHOOKSQUEUE,&clearpendingkicks);

  for (pk=pendingkicklist;pk;pk=pendingkicklist) {
    pendingkicklist = pk->next;
    freesstring(pk->reason);
    free(pk);
  }
}

void luc_handlekick(int hooknum, void *arg) {
  void **args=arg;
  nick *target=args[1];
  void *myargs[3];
  
  if (homeserver(target->numeric)!=mylongnum)
    return;
    
  if (umhandlers[target->numeric & MAXLOCALUSER]) {
    myargs[0]=args[2];
    myargs[1]=args[0];
    myargs[2]=args[3];
    
    (umhandlers[target->numeric & MAXLOCALUSER])(target, LU_KICKED, myargs);
  }
}

/* invites look something like:
 * XXyyy I TargetNick :#channel
 */

int handleinvitecmd(void *source, int cargc, char **cargv) {
  void *nargs[2];
  nick *sender;
  channel *cp;
  nick *target;
  
  if (cargc<2) {
    return CMD_OK;
  }
  
  if (!(sender=getnickbynumericstr(source))) {
    Error("localuserchannel",ERR_WARNING,"Got invite from unknown numeric %s.",source);
    return CMD_OK;
  }
  
  if (!(target=getnickbynick(cargv[0]))) {
    Error("localuserchannel",ERR_WARNING,"Got invite for unknown local user %s.",cargv[0]);
    return CMD_OK;
  }
  
  if (!(cp=findchannel(cargv[1]))) {
    Error("localuserchannel",ERR_WARNING,"Got invite for non-existent channel %s.",cargv[1]);
    return CMD_OK;
  }
  
  if (homeserver(target->numeric) != mylongnum) {
    Error("localuserchannel",ERR_WARNING,"Got invite for non-local user %s.",target->nick);
    return CMD_OK;
  }
  
  /* This is a valid race condition.. */
  if (getnumerichandlefromchanhash(cp->users, target->numeric)) {
    Error("localuserchannel",ERR_DEBUG,"Got invite for user %s already on %s.",target->nick, cp->index->name->content);
    return CMD_OK;
  }
  
  nargs[0]=(void *)sender;
  nargs[1]=(void *)cp;
  
  if (umhandlers[target->numeric&MAXLOCALUSER]) {
    (umhandlers[target->numeric&MAXLOCALUSER])(target, LU_INVITE, nargs);
  }
  
  return CMD_OK;
}

/* PRIVMSG/NOTICE to channel handling is identical up to the point where the hook is called. */
static int handlechannelmsgornotice(void *source, int cargc, char **cargv, int isnotice) {
  void *nargs[3];
  nick *sender;
  channel *target;
  nick *np;
  unsigned long numeric;
  int i;
  int found=0;

  if (cargc<2) {
    return CMD_OK;
  }
  
  if (cargv[0][0]!='#' && cargv[0][0]!='+') {
    /* Not a channel message */
    return CMD_OK;
  }
  
  if ((sender=getnickbynumericstr((char *)source))==NULL) {
    Error("localuserchannel",ERR_DEBUG,"PRIVMSG/NOTICE from non existant user %s",(char *)source);
    return CMD_OK;
  }

  if ((target=findchannel(cargv[0]))==NULL) {
    Error("localuserchannel",ERR_DEBUG,"PRIVMSG/NOTICE to non existant channel %s",cargv[0]);
    return CMD_OK;
  }

  /* OK, we have a valid channel the message was sent to.  Let's look to see
   * if we have any local users on there.  Set up the arguments first as they are
   * always going to be the same. */

  nargs[0]=(void *)sender;
  nargs[1]=(void *)target;
  nargs[2]=(void *)cargv[1];

  for (found=0,i=0;i<target->users->hashsize;i++) {
    numeric=target->users->content[i];
    if (numeric!=nouser && homeserver(numeric&CU_NUMERICMASK)==mylongnum) {
      /* OK, it's one of our users.. we need to deal with it */
      if (found && ((getnickbynumericstr((char *)source)==NULL) || ((findchannel(cargv[0]))==NULL) || !(getnumerichandlefromchanhash(target->users, sender->numeric)))) {
        Error("localuserchannel", ERR_INFO, "Nick or channel lost, or user no longer on channel in LU_CHANMSG");
        break;
      }
      found++;
      if (umhandlers[numeric&MAXLOCALUSER]) {
	if ((np=getnickbynumeric(numeric))==NULL) {
          Error("localuserchannel",ERR_ERROR,"PRIVMSG/NOTICE to channel user who doesn't exist (?) on %s",cargv[0]);
          continue;
        }
	if (!IsDeaf(np)) {
          (umhandlers[numeric&MAXLOCALUSER])(np,(isnotice?LU_CHANNOTICE:LU_CHANMSG),nargs);
        } else {
          found--;
        }
      }
    }
  }

  if (!found) {
    Error("localuserchannel",ERR_DEBUG,"Couldn't find any local targets for PRIVMSG/NOTICE to %s",cargv[0]);
  }

  return CMD_OK;
}

/* Wrapper functions to call the above code */
int handlechannelmsgcmd(void *source, int cargc, char **cargv) {
  return handlechannelmsgornotice(source, cargc, cargv, 0);
}

int handlechannelnoticecmd(void *source, int cargc, char **cargv) {
  return handlechannelmsgornotice(source, cargc, cargv, 1);
}

/* Burst onto channel.  This replaces the timestamp and modes
 * with the provided ones.  Keys and limits use the provided ones 
 * if needed.  nick * is optional, but joins the channel opped if 
 * provided. 
 * 
 * Due to the way ircu works, this only works if the provided timestamp is
 * older than the one currently on the channel.  If the timestamps are
 * equal, the modes are ignored, but the user (if any) is still allowed to
 * join with op.  If the provided timestamp is newer than the exsting one we
 * just do a join instead - if you try to replace an old timestamp with a
 * newer one ircu will just laugh at you (and you will be desynced).
 */
int localburstontochannel(channel *cp, nick *np, time_t timestamp, flag_t modes, unsigned int limit, char *key) {
  unsigned int i;
  char extramodebuf[512];
  char nickbuf[512];
  
  if (cp==NULL)
    return 1;
    
  if (timestamp > cp->timestamp) {
    return localjoinchannel(np, cp);
  }
  
  if (timestamp < cp->timestamp) {
    cp->timestamp=timestamp;
    cp->flags=modes;
    
    /* deal with key - if we need one use the provided one if set, otherwise
     * the existing one, but if there is no existing one clear +k */
    if (IsKey(cp)) {
      if (key) {
        /* Free old key, if any */
        if (cp->key)
          freesstring(cp->key);

        cp->key=getsstring(key,KEYLEN);
      } else {
        if (!cp->key)
          ClearKey(cp);
      }
    } else {
      /* Not +k - free the existing key, if any */
      freesstring(cp->key);
      cp->key=NULL;
    }
    
    if (IsLimit(cp)) {
      if (limit) {
        cp->limit=limit;
      } else {
        if (!cp->limit)
          ClearLimit(cp);
      } 
    } else {
      if (cp->limit)
        cp->limit=0;
    }
    
    /* We also need to blow away all other op/voice and bans on the
     * channel.  This is the same code we use when someone else does 
     * it to us. */ 
    clearallbans(cp); 
    for (i=0;i<cp->users->hashsize;i++) {
      if (cp->users->content[i]!=nouser) {
        cp->users->content[i]&=CU_NUMERICMASK;
      }
    }
  }

  /* Actually add the nick to the channel.  Make sure it's a local nick and actually exists first. */
  if (np && (homeserver(np->numeric) == mylongnum) &&
      !(getnumerichandlefromchanhash(cp->users,np->numeric))) {
    addnicktochannel(cp,(np->numeric)|CUMODE_OP);
  } else {
    np=NULL; /* If we're not adding it here, don't send it later in the burst msg either */
  }
  
  if (connected) {
    /* actual burst message */
    if (np) {
      sprintf(nickbuf," %s:o", longtonumeric(np->numeric,5));
    } else {
      nickbuf[0]='\0';
    }
    
    if (IsLimit(cp)) {
      sprintf(extramodebuf," %d",cp->limit);
    } else {
      extramodebuf[0]='\0';
    }
    
    /* XX B #channel <timestamp> +modes <limit> <key> <user> */
    irc_send("%s B %s %lu %s %s%s%s%s",
              mynumeric->content,cp->index->name->content,cp->timestamp,
              printflags(cp->flags,cmodeflags),extramodebuf,
              IsKey(cp)?" ":"",IsKey(cp)?cp->key->content:"", nickbuf);
  }

  /* Tell the world something happened... */
  triggerhook(HOOK_CHANNEL_BURST,cp);

  return 0;
}

int localjoinchannel(nick *np, channel *cp) {
  void *harg[2];
  
  if (cp==NULL || np==NULL) {
    return 1;
  }

  /* Check that the user _is_ a local one.. */
  if (homeserver(np->numeric)!=mylongnum) {
    return 1;
  }
  
  /* Check that the user isn't on the channel already */
  if ((getnumerichandlefromchanhash(cp->users,np->numeric))!=NULL) {
    return 1;
  }

  /* OK, join the channel.. */
  addnicktochannel(cp,np->numeric);

  /* Trigger the event */
  harg[0]=cp;
  harg[1]=np;
    
  triggerhook(HOOK_CHANNEL_JOIN, harg);
    
  if (connected) {
    irc_send("%s J %s %lu",longtonumeric(np->numeric,5),cp->index->name->content,cp->timestamp);
  }
  return 0;
}

int localpartchannel(nick *np, channel *cp) {
  void *harg[3];
  
  /* Check pointers are valid.. */
  if (cp==NULL || np==NULL) {
    Error("localuserchannel",ERR_WARNING,"Trying to part NULL channel or NULL nick (cp=%x,np=%x)",cp,np);
    return 1;
  }	
  
  /* And that user is local.. */
  if (homeserver(np->numeric)!=mylongnum) {
    Error("localuserchannel",ERR_WARNING,"Trying to part remote user %s",np->nick);
    return 1;
  }
  
  /* Check that user is on channel */
  if (getnumerichandlefromchanhash(cp->users,np->numeric)==NULL) {
    Error("localuserchannel",ERR_WARNING,"Trying to part user %s from channel %s it is not on",np->nick,cp->index->name->content);
    return 1;
  }

  if (connected) {
    irc_send("%s L %s",longtonumeric(np->numeric,5),cp->index->name->content);
  }
  
  harg[0]=cp;
  harg[1]=np;
  harg[2]=NULL;
  triggerhook(HOOK_CHANNEL_PART,harg);
  
  /* Now leave the channel */
  delnickfromchannel(cp,np->numeric,1);
  
  return 0;
}  

int localcreatechannel(nick *np, char *channame) {
  channel *cp;
  
  /* Check that the user _is_ a local one.. */
  if (homeserver(np->numeric)!=mylongnum) {
    return 1;
  }

  if ((cp=findchannel(channame))!=NULL) {
    /* Already exists */
    return 1;
  }
  
  cp=createchannel(channame);
  cp->timestamp=getnettime();
  
  /* Add the local user to the channel, preopped */
  addnicktochannel(cp,(np->numeric)|CUMODE_OP);
  
  if (connected) {
    irc_send("%s C %s %ld",longtonumeric(np->numeric,5),cp->index->name->content,cp->timestamp);
  }

  triggerhook(HOOK_CHANNEL_NEWCHANNEL,(void *)cp);
  return 0;
}
  
int localgetops(nick *np, channel *cp) {
  unsigned long *lp;
  
  /* Check that the user _is_ a local one.. */
  if (homeserver(np->numeric)!=mylongnum) {
    return 1;
  }
  
  if ((lp=getnumerichandlefromchanhash(cp->users,np->numeric))==NULL) {
    return 1;
  }
  
  if (*lp & CUMODE_OP) {
    /* already opped */
    return 1;
  }
  
  /* Op the user */
  (*lp)|=CUMODE_OP;
  
  if (connected) {
    irc_send("%s M %s +o %s",mynumeric->content,cp->index->name->content,longtonumeric(np->numeric,5));
  }

  return 0;
}

int localgetvoice(nick *np, channel *cp) {
  unsigned long *lp;
  
  /* Check that the user _is_ a local one.. */
  if (homeserver(np->numeric)!=mylongnum) {
    return 1;
  }
  
  if ((lp=getnumerichandlefromchanhash(cp->users,np->numeric))==NULL) {
    return 1;
  }
  
  if (*lp & CUMODE_VOICE) {
    /* already opped */
    return 1;
  }
  
  /* Voice the user */
  (*lp)|=CUMODE_VOICE;
  
  if (connected) {
    irc_send("%s M %s +v %s",mynumeric->content,cp->index->name->content,longtonumeric(np->numeric,5));
  }

  return 0;
}

/*
 * localsetmodeinit:
 *  Initialises the modechanges structure.
 */

void localsetmodeinit (modechanges *changes, channel *cp, nick *np) {
  changes->cp=cp;
  changes->source=np;
  changes->changecount=0;
  changes->addflags=0;
  changes->delflags=0;
}

/*
 * localdosetmode_ban:
 *  Set or clear a ban on the channel
 */

void localdosetmode_ban (modechanges *changes, const char *ban, short dir) {
  sstring *bansstr=getsstring(ban,HOSTLEN+NICKLEN+USERLEN+5);
  
  /* If we're told to clear a ban that isn't here, do nothing. */
  if (dir==MCB_DEL && !clearban(changes->cp, bansstr->content, 1))
    return;
    
  /* Similarly if someone is trying to add a completely overlapped ban, do
   * nothing */
  if (dir==MCB_ADD && !setban(changes->cp, bansstr->content))
    return;

  if (changes->changecount >= MAXMODEARGS)
    localsetmodeflush(changes, 0);
  
  changes->changes[changes->changecount].str=bansstr;
  changes->changes[changes->changecount].flag='b';
  changes->changes[changes->changecount++].dir=dir;

  if (dir==MCB_ADD) {
    setban(changes->cp, bansstr->content);
  } 
}

/*
 * localdosetmode_key:
 *  Set or clear a key on the channel
 */

void localdosetmode_key (modechanges *changes, const char *key, short dir) {
  int i,j;
  sstring *keysstr;

  /* Check we have space in the buffer */
  if (changes->changecount >= MAXMODEARGS)
    localsetmodeflush(changes,0);

  if (dir==MCB_ADD) {
    /* Get a copy of the key for use later */
    keysstr=getsstring(key, KEYLEN);
    
    /* Check there isn't a key set/clear in the pipeline already.. */
    for (i=0;i<changes->changecount;i++) {
      if (changes->changes[i].flag=='k') {
	/* There's a change already.. */
	if (changes->changes[i].dir==MCB_ADD) {
	  /* Already an add key change.  Here, we just replace the key
	   * we were going to add with this one.  Note that we need to 
	   * free the current cp->key, and the changes.str */
	  freesstring(changes->changes[i].str);
	  changes->changes[i].str=keysstr;
	  freesstring(changes->cp->key);
	  changes->cp->key=getsstring(key, KEYLEN);
	  /* That's it, we're done */
	  return;
	} else {
	  /* There was a command to delete key.. we need to flush 
	   * this out then add the new key (-k+k isn't valid).
	   * Make sure this gets flushed, then drop through to common code */
	  localsetmodeflush(changes, 1);
	}
      }
    }
    
    /* We got here, so there's no change pending already .. check for key on chan */
    if (IsKey(changes->cp)) {
      if (sstringcompare(changes->cp->key, keysstr)) {
	/* Key is set and different.  Need to put -k in and flush changes now */
	changes->changes[changes->changecount].str=changes->cp->key; /* implicit free */
	changes->changes[changes->changecount].dir=MCB_DEL;
	changes->changes[changes->changecount++].flag='k';
	localsetmodeflush(changes, 1); /* Note that this will free the sstring on the channel */
      } else {
	/* Key is set and the same: do nothing. */
	freesstring(keysstr); /* Don't need this after all */
	return;
      }
    }
    
    /* If we get here, there's no key on the channel right now and nothing in the buffer to
     * add or remove one */
    SetKey(changes->cp);
    changes->cp->key=getsstring(key, KEYLEN);
    
    changes->changes[changes->changecount].str=keysstr;
    changes->changes[changes->changecount].dir=MCB_ADD;
    changes->changes[changes->changecount++].flag='k';
  } else {
    /* We're removing a key.. */
    /* Only bother if key is set atm */
    if (IsKey(changes->cp)) {
      ClearKey(changes->cp);
      for(i=0;i<changes->changecount;i++) {
	if (changes->changes[i].flag=='k') {
	  /* We were already doing something with a key.. 
	   * it MUST be adding one */
	  assert(changes->changes[i].dir==MCB_ADD);
	  /* Just forget the earlier change */
	  freesstring(changes->changes[i].str);
	  changes->changecount--;
	  for (j=i;j<changes->changecount;j++) {
	    changes->changes[j]=changes->changes[j+1];
	  }
	  /* Explicitly free key on chan */
	  freesstring(changes->cp->key);
	  changes->cp->key=NULL;
	  return;
	}
      }

      /* We didn't hit a key change, so put a remove command in */
      changes->changes[changes->changecount].str=changes->cp->key; /* implicit free */
      changes->cp->key=NULL;
      changes->changes[changes->changecount].dir=MCB_DEL;
      changes->changes[changes->changecount++].flag='k';
    }
  }
}

void localdosetmode_limit (modechanges *changes, unsigned int limit, short dir) {
  int i;
  char buf[20];

  if (dir==MCB_DEL) {
    localdosetmode_simple(changes, 0, CHANMODE_LIMIT);
    return;
  }

  /* Kill redundant changes */
  if ((IsLimit(changes->cp) && changes->cp->limit==limit) || !limit)
    return;

  SetLimit(changes->cp);
  changes->cp->limit=limit;
  changes->delflags &= ~CHANMODE_LIMIT;
  
  /* Check for existing limit add */
  for (i=0;i<changes->changecount;i++) {
    if (changes->changes[i].flag=='l') {
      /* must be add */
      freesstring(changes->changes[i].str);
      sprintf(buf,"%u",limit);
      changes->changes[i].str=getsstring(buf,20);
      return;
    }
  }

  /* None, add new one.  Note that we can do +l even if a limit is already set */
  if (changes->changecount >= MAXMODEARGS)
    localsetmodeflush(changes,0);
  
  changes->changes[changes->changecount].flag='l';
  sprintf(buf,"%u",limit);
  changes->changes[changes->changecount].str=getsstring(buf,20);
  changes->changes[changes->changecount++].dir=MCB_ADD;
}

void localdosetmode_simple (modechanges *changes, flag_t addmodes, flag_t delmodes) {
  int i,j;

  /* We can't add a mode we're deleting, a key, limit, or mode that's already set */
  addmodes &= ~(delmodes | CHANMODE_KEY | CHANMODE_LIMIT | changes->cp->flags);

  /* We can't delete a key or a mode that's not set */
  delmodes &= (~(CHANMODE_KEY) & changes->cp->flags);
  
  /* If we're doing +p, do -s as well, and vice versa */
  /* Also disallow +ps */
  if (addmodes & CHANMODE_SECRET) {
    addmodes &= ~(CHANMODE_PRIVATE);
    delmodes |= (CHANMODE_PRIVATE & changes->cp->flags);
  }

  if (addmodes & CHANMODE_PRIVATE) {
    delmodes |= (CHANMODE_SECRET & changes->cp->flags);
  }

  /* Fold changes into channel */
  changes->cp->flags |= addmodes;
  changes->cp->flags &= ~delmodes;

  if (delmodes & CHANMODE_LIMIT) {
    /* Check for +l in the parametered changes */
    for (i=0;i<changes->changecount;i++) {
      if (changes->changes[i].flag=='l') {
	freesstring(changes->changes[i].str);
	changes->changecount--;
	for (j=0;j<changes->changecount;j++) {
	  changes->changes[j]=changes->changes[j+1];
	}
      }
      break;
    }
    changes->cp->limit=0;
  }

  /* And into the changes buffer */
  changes->addflags &= ~delmodes;
  changes->addflags |= addmodes;

  changes->delflags &= ~addmodes;
  changes->delflags |= delmodes;
}

/*
 * localdosetmode:
 *  Applies a mode change.
 */

void localdosetmode_nick (modechanges *changes, nick *target, short modes) {
  unsigned long *lp;
  
  if ((lp=getnumerichandlefromchanhash(changes->cp->users,target->numeric))==NULL) {
    /* Target isn't on channel, abort */
    return;
  }

  if ((modes & MC_DEOP) && (*lp & CUMODE_OP)) {
    (*lp) &= ~CUMODE_OP;
    if (changes->changecount >= MAXMODEARGS)
      localsetmodeflush(changes, 0);
    changes->changes[changes->changecount].str=getsstring(longtonumeric(target->numeric,5),5);
    changes->changes[changes->changecount].dir=MCB_DEL;
    changes->changes[changes->changecount++].flag='o';
  }

  if ((modes & MC_DEVOICE) && (*lp & CUMODE_VOICE)) {
    (*lp) &= ~CUMODE_VOICE;
    if (changes->changecount >= MAXMODEARGS)
      localsetmodeflush(changes, 0);
    changes->changes[changes->changecount].str=getsstring(longtonumeric(target->numeric,5),5);
    changes->changes[changes->changecount].dir=MCB_DEL;
    changes->changes[changes->changecount++].flag='v';
  }

  if ((modes & MC_OP) && !(modes & MC_DEOP) && !(*lp & CUMODE_OP)) {
    (*lp) |= CUMODE_OP;
    if (changes->changecount >= MAXMODEARGS)
      localsetmodeflush(changes, 0);
    changes->changes[changes->changecount].str=getsstring(longtonumeric(target->numeric,5),5);
    changes->changes[changes->changecount].dir=MCB_ADD;
    changes->changes[changes->changecount++].flag='o';
  }

  if ((modes & MC_VOICE) && !(modes & MC_DEVOICE) && !(*lp & CUMODE_VOICE)) {
    (*lp) |= CUMODE_VOICE;
    if (changes->changecount >= MAXMODEARGS)
      localsetmodeflush(changes, 0);
    changes->changes[changes->changecount].str=getsstring(longtonumeric(target->numeric,5),5);
    changes->changes[changes->changecount].dir=MCB_ADD;
    changes->changes[changes->changecount++].flag='v';
  }
	
}

/*
 * localsetmodeflush:
 *  Sends out mode changes to the network.
 */

void localsetmodeflush (modechanges *changes, int flushall) {
  int i,j=0;
  unsigned long *lp;
  char source[6];

  char addmodes[26];
  int ampos=0;
  char remmodes[26];
  int rmpos=0;

  char addargs[500];
  int aapos=0;
  char remargs[500];
  int rapos=0;

  strcpy(addmodes, printflags_noprefix(changes->addflags, cmodeflags));
  ampos=strlen(addmodes);

  strcpy(remmodes, printflags_noprefix(changes->delflags, cmodeflags));
  rmpos=strlen(remmodes);

  changes->addflags=changes->delflags=0;

  for (i=0;i<changes->changecount;i++) {
    /* Don't overflow the string, kinda nasty to work out.. format is: */
    /* AAAA M #chan +add-rem (addstr) (remstr) */
    if ((changes->cp->index->name->length + aapos + rapos + 
         ampos + rmpos + changes->changes[i].str->length + 20) > BUFSIZE) 
      break;

    switch (changes->changes[i].dir) {
    case MCB_ADD:
      addmodes[ampos++]=changes->changes[i].flag;
      aapos+=sprintf(addargs+aapos, "%s ", changes->changes[i].str->content);
      break;

    case MCB_DEL:
      remmodes[rmpos++]=changes->changes[i].flag;
      rapos+=sprintf(remargs+rapos, "%s ", changes->changes[i].str->content);
      break;
    }
    freesstring(changes->changes[i].str);
  }

  if (i<changes->changecount) {
    for (j=i;j<changes->changecount;j++)
      changes->changes[j-i]=changes->changes[j];
  }

  changes->changecount -= i;

  if ((ampos+rmpos)==0) {
    /* No changes */
    return;
  }

  addmodes[ampos]='\0';
  remmodes[rmpos]='\0';
  addargs[aapos]='\0';
  remargs[rapos]='\0';

  if (changes->source==NULL || 
      (lp=getnumerichandlefromchanhash(changes->cp->users,changes->source->numeric))==NULL) {
    /* User isn't on channel, hack mode */
    strcpy(source,mynumeric->content);
  } else {
    /* Check the user is local */
    if (homeserver(changes->source->numeric)!=mylongnum) {
      return;
    }
    if ((*lp&CUMODE_OP)==0) {
      localgetops(changes->source,changes->cp);
    }
    strcpy(source,longtonumeric(changes->source->numeric,5));
  }

  if (connected) {
    irc_send("%s M %s %s%s%s%s %s%s",source,changes->cp->index->name->content,
	     rmpos ? "-" : "", remmodes,
	     ampos ? "+" : "", addmodes, remargs, addargs);
  }

  /* If we have to flush everything out but didn't finish, go round again */
  if (changes->changecount && flushall) 
    localsetmodeflush(changes, 1);
}

/*
 * localsetmodes:
 *  Sets modes for the user on the channel.  This is now a stub routine that 
 *  uses the new functions.
 */

int localsetmodes(nick *np, channel *cp, nick *target, short modes) {
  modechanges changes;

  localsetmodeinit (&changes, cp, np);
  localdosetmode_nick (&changes, target, modes);
  localsetmodeflush (&changes, 1);
  
  return 0;
}

/* This function just sends the actual mode change, 
 * it assumes that the actual channel modes have been changed appropriately.
 * This is unfortunately inconsistent with the rest of the localuser API..
 */

void localusermodechange(nick *np, channel *cp, char *modes) {
  char source[10];
  unsigned long *lp;

  if (np==NULL || (lp=getnumerichandlefromchanhash(cp->users,np->numeric))==NULL) {
    /* User isn't on channel, hack mode */
    strcpy(source,mynumeric->content);
  } else {
    /* Check the user is local */
    if (homeserver(np->numeric)!=mylongnum) {
      return;
    }
    if ((*lp&CUMODE_OP)==0) {
      localgetops(np,cp);
    }
    strcpy(source,longtonumeric(np->numeric,5));
  }

  if (connected) {
    irc_send("%s M %s %s",source,cp->index->name->content,modes);
  }
}

/* This function actually sets the topic itself though.. a bit inconsistent :/ */

void localsettopic(nick *np, channel *cp, char *topic) {
  unsigned long *lp;
  char source[10];

  if (np==NULL || (lp=getnumerichandlefromchanhash(cp->users,np->numeric))==NULL) {
    /* User isn't on channel, hack mode */
    strcpy(source,mynumeric->content);
  } else {
    /* Check the user is local */
    if (homeserver(np->numeric)!=mylongnum) {
      return;
    }
    if ((*lp&CUMODE_OP)==0 && IsTopicLimit(cp)) {
      localgetops(np,cp);
    }
    strcpy(source,longtonumeric(np->numeric,5));
  }

  if (cp->topic) {
    freesstring(cp->topic);
  }

  cp->topic=getsstring(topic,TOPICLEN);
  cp->topictime=getnettime();
  
  if (connected) {
    irc_send("%s T %s %u %u :%s",source,cp->index->name->content,cp->timestamp,cp->topictime,(cp->topic)?cp->topic->content:"");
  }
}

void localkickuser(nick *np, channel *cp, nick *target, const char *message) {
  pendingkick *pk;

  if (hookqueuelength) {
    for (pk = pendingkicklist; pk; pk = pk->next)
      if (pk->target == target && pk->chan == cp)
        return;

    Error("localuserchannel", ERR_DEBUG, "Adding pending kick for %s on %s", target->nick, cp->index->name->content);
    pk = (pendingkick *)malloc(sizeof(pendingkick));
    pk->source = np;
    pk->chan = cp;
    pk->target = target;
    pk->reason = getsstring(message, BUFSIZE);
    pk->next = pendingkicklist;
    pendingkicklist = pk;
  } else {
    _localkickuser(np, cp, target, message);
  }
}

void _localkickuser(nick *np, channel *cp, nick *target, const char *message) {
  unsigned long *lp;
  char source[10];

  if (np==NULL || (lp=getnumerichandlefromchanhash(cp->users,np->numeric))==NULL) {
    /* User isn't on channel, hack mode */
    strcpy(source,mynumeric->content);
  } else {
    /* Check the user is local */
    if (homeserver(np->numeric)!=mylongnum) {
      return;
    }
    if ((*lp&CUMODE_OP)==0) {
      localgetops(np,cp);
    }
    strcpy(source,longtonumeric(np->numeric,5));
  }

  if ((lp=getnumerichandlefromchanhash(cp->users,target->numeric))==NULL)
    return;

  /* Send the message to the network first in case delnickfromchannel() 
   * destroys the channel.. */
  if (connected) {
    irc_send("%s K %s %s :%s",source,cp->index->name->content,
	     longtonumeric(target->numeric,5), message);
  }

  delnickfromchannel(cp, target->numeric, 1);
}

void clearpendingkicks(int hooknum, void *arg) {
  pendingkick *pk;

  pk = pendingkicklist;
  while (pk) {
    pendingkicklist = pk->next;

    if (pk->target && pk->chan) {
      Error("localuserchannel", ERR_DEBUG, "Processing pending kick for %s on %s", pk->target->nick, pk->chan->index->name->content);
      _localkickuser(pk->source, pk->chan, pk->target, pk->reason->content);
    }

    freesstring(pk->reason);
    free(pk);
    pk = pendingkicklist;
  }
}

void checkpendingkicknicks(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  pendingkick *pk;

  for (pk=pendingkicklist; pk; pk = pk->next) {
    if (pk->source == np) {
      Error("localuserchannel", ERR_INFO, "Pending kick source %s got deleted, NULL'ing source for pending kick", np->nick);
      pk->source = NULL;
    }
    if (pk->target == np) {
      Error("localuserchannel", ERR_INFO, "Pending kick target %s got deleted, NULL'ing target for pending kick", np->nick);
      pk->target = NULL;
    }
  }
}

void checkpendingkickchannels(int hooknum, void *arg) {
  channel *cp = (channel *)arg;
  pendingkick *pk;

  for (pk=pendingkicklist; pk; pk = pk->next) {
    if (pk->chan == cp) {
      Error("localuserchannel", ERR_INFO, "Pending kick channel %s got deleted, NULL'ing channel for pending kick", cp->index->name->content);
      pk->chan = NULL;
    }
  }
}

void sendmessagetochannel(nick *source, channel *cp, char *format, ... ) {
  char buf[BUFSIZE];
  char senderstr[6];
  va_list va;
  
  if (!source)
    return;
 
  longtonumeric2(source->numeric,5,senderstr);
     
  va_start(va,format);
  /* 10 bytes of numeric, 5 bytes of fixed format + terminator = 17 bytes */
  /* So max sendable message is 495 bytes.  Of course, a client won't be able
   * to receive this.. */

  vsnprintf(buf,BUFSIZE-17,format,va);
  va_end(va);

  if (connected) {
    irc_send("%s P %s :%s",senderstr,cp->index->name->content,buf);
  }
}

void localinvite(nick *source, channel *cp, nick *target) {

  /* Servers can't send invites */
  if (!source) 
    return;

  /* CHECK: Does the sender have to be on the relevant channel? */
  
  /* For some reason invites are sent with the target nick as
   * argument */
  if (connected) {
    irc_send("%s I %s :%s",longtonumeric(source->numeric,5),
	     target->nick, cp->index->name->content);
  }
}
  
