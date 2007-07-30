/* channel.c */

#include <stdio.h>
#include <string.h>

#include "channel.h"
#include "../server/server.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../parser/parser.h"
#include "../irc/irc.h"
#include "../lib/base64.h"
#include "../lib/strlfunc.h"

int handleburstmsg(void *source, int cargc, char **cargv) {
  channel *cp;
  time_t timestamp;
  int wipeout=0;
  int i;
  int arg=0;
  char *charp;
  int newlimit;
  int waslimit,waskeyed;
  char *nextnum;
  unsigned long currentmode;
  int isnewchan;
  
  /* (we don't see the first 2 params in cargc) */
  /* AK B #+lod+ 1017561154 +tnk eits ATJWu:o,AiW1a,Ag3lV,AiWnl,AE6oI :%*!@D577A90D.kabel.telenet.be */
  
  if (cargc<2) {
    Error("channel",ERR_WARNING,"Burst message with only %d parameters",cargc);
    return CMD_OK;
  }
  
  timestamp=strtol(cargv[1],NULL,10);
  
  if ((cp=findchannel(cargv[0]))==NULL) {
    /* We don't have this channel already */
    cp=createchannel(cargv[0]);
    cp->timestamp=timestamp;
    isnewchan=1;
  } else {
    isnewchan=0;
    if (timestamp<cp->timestamp) {
      /* The incoming timestamp is older.  Erase all our current channel modes, and the topic. */
      cp->timestamp=timestamp;
      freesstring(cp->topic);
      cp->topic=NULL;
      cp->topictime=0;
      freesstring(cp->key);
      cp->key=NULL;
      cp->limit=0;
      cp->flags=0;
      clearallbans(cp);
      /* Remove all +v, +o we currently have */
      for(i=0;i<cp->users->hashsize;i++) {
        if (cp->users->content[i]!=nouser) {
          cp->users->content[i]&=CU_NUMERICMASK;
        }
      }
    } else if (timestamp>cp->timestamp) {
      /* The incoming timestamp is greater.  Ignore any incoming modes they may happen to set */
      wipeout=1;
    }
  }

  /* OK, dealt with the name and timestamp. 
   * Loop over the remaining args */
  for (arg=2;arg<cargc;arg++) {
    if (cargv[arg][0]=='+') {
      /* Channel modes */
      if (wipeout) {
        /* We ignore the modes, but we need to see if they include +l or +k 
         * so that we can ignore their corresponding values */
        for (charp=cargv[arg];*charp;charp++) {
          if (*charp=='k' || *charp=='l') {
            arg++;
          }
        }
      } else {
        /* Clear off the limit and key flags before calling setflags so we can see if the burst tried to set them */
        /* If the burst doesn't set them, we restore them afterwards */
        waslimit=IsLimit(cp); ClearLimit(cp);
        waskeyed=IsKey(cp);   ClearKey(cp);
        /* We can then use the flag function for these modes */
        setflags(&(cp->flags),CHANMODE_ALL,cargv[arg],cmodeflags,REJECT_NONE);
        /* Pick up the limit and key, if they were set.  Note that the limit comes first */
        if (IsLimit(cp)) { /* A limit was SET by the burst */
          if (++arg>=cargc) {
            /* Ran out of args -- damn ircd is spewing out crap again */
            Error("channel",ERR_WARNING,"Burst +l with no argument");
            break; /* "break" being the operative word */
          } else {
            newlimit=strtol(cargv[arg],NULL,10);
          }
          if (cp->limit>0 && waslimit) {
            /* We had a limit before -- we now have the lowest one of the two */
            if (newlimit<cp->limit) {
              cp->limit=newlimit;
            }
          } else {
            /* No limit before -- we just have the new one */
            cp->limit=newlimit;
          }
        } else if (waslimit) {
          SetLimit(cp); /* We had a limit before, but the burst didn't set one.  Restore flag. */
        }
        
        if (IsKey(cp)) { /* A key was SET by the burst */
          if (++arg>=cargc) {
            /* Ran out of args -- oopsie! */
            Error("channel",ERR_WARNING,"Burst +k with no argument");
            break;
          }
          if (waskeyed) {
            /* We had a key before -- alphabetically first wins */
            if (ircd_strcmp(cargv[arg],cp->key->content)<0) {
              /* Replace our key */
              freesstring(cp->key);
              cp->key=getsstring(cargv[arg],KEYLEN);
            }
          } else {
            /* No key before -- just the new one */
            cp->key=getsstring(cargv[arg],KEYLEN);
          }
        } else if (waskeyed) {
          SetKey(cp); /* We had a key before, but the burst didn't set one.  Restore flag. */
        }
      }       
    } else if (cargv[arg][0]=='%') {
      /* We have one or more bans here */
      nextnum=cargv[arg]+1;
      while (*nextnum) {
        /* Split off the next ban */
        for (charp=nextnum;*charp;charp++) {
          if (*charp==' ') {
            *charp='\0';
            charp++;
            break;
          }
        }        
        setban(cp,nextnum);
        nextnum=charp;
      }
    } else {
      /* List of numerics */
      nextnum=charp=cargv[arg];
      currentmode=0;
      while (*nextnum!='\0') {
        /* Step over the next numeric */
        for (i=0;i<5;i++) {
          if (*charp++=='\0')
            break;
        }
        if (i<5) {
          break;
        }
        if (*charp==',') {
          *charp='\0';
          charp++;
        } else if (*charp==':') {
          *charp='\0';
          charp++;
          currentmode=0;
          /* Look for modes */
          for (;*charp;charp++) {
            if (*charp=='v') {
              currentmode|=CUMODE_VOICE;
            } else if (*charp=='o') {
              currentmode|=CUMODE_OP;
            } else if (*charp==',') {
              charp++;
              break;
            }
          }
          /* If we're ignore incoming modes, zap it to zero again */
          if (wipeout) {
            currentmode=0;
          }
        }
        /* OK.  At this point charp points to either '\0' if we're at the end,
         * or the start of the next numeric otherwise.  nextnum points at a valid numeric
         * we need to add, and currentmode reflects the correct mode */
        addnicktochannel(cp,(numerictolong(nextnum,5)|currentmode));
        nextnum=charp;
      }
    }
  }
  if (cp->users->totalusers==0) {
    /* Oh dear, the channel is now empty.  Perhaps one of those 
     * charming empty burst messages you get sometimes.. */
    if (!isnewchan) {
      /* I really don't think this can happen, can it..? */
      /* Only send the LOSTCHANNEL if the channel existed before */
      triggerhook(HOOK_CHANNEL_LOSTCHANNEL,cp);
    }
    delchannel(cp);
  } else {
    /* If this is a new channel, we do the NEWCHANNEL hook also */
    if (isnewchan) {
      triggerhook(HOOK_CHANNEL_NEWCHANNEL,cp);
    }
    /* Just one hook to say "something happened to this channel" */
    triggerhook(HOOK_CHANNEL_BURST,cp);       
  }
   
  return CMD_OK;     
}

int handlejoinmsg(void *source, int cargc, char **cargv) {
  char *pos,*nextchan;
  nick *np;
  void *harg[2];
  channel *cp,**ch;
  long timestamp=0;
  int i;
  int newchan=0;
  
  if (cargc<1) {
    return CMD_OK;
  }
  
  if (cargc>1) {
    /* We must have received a timestamp too */
    timestamp=strtol(cargv[1],NULL,10);
  }
  
  /* Find out who we are talking about here */
  np=getnickbynumericstr(source);
  if (np==NULL) {
    Error("channel",ERR_WARNING,"Channel join from non existent user %s",source);
    return CMD_OK;  
  }
  
  nextchan=pos=cargv[0];
  while (*nextchan!='\0') {
    /* Find the next chan position */
    for (;*pos!='\0' && *pos!=',';pos++)
      ; /* Empty loop */
  
    if (*pos==',') {
      *pos='\0';
      pos++;
    }
    
    /* OK, pos now points at either null or the next chan 
     * and nextchan now points at the channel name we want to parse next */
     
    if(nextchan[0]=='0' && nextchan[1]=='\0') {
      /* Leave all channels
       * We do this as if they were leaving the network, 
       * then vape their channels array and start again */
      ch=(channel **)(np->channels->content);
      for(i=0;i<np->channels->cursi;i++) {
        /* Send hook */
        harg[0]=ch[i];
        harg[1]=np;
        triggerhook(HOOK_CHANNEL_PART,harg);
        delnickfromchannel(ch[i],np->numeric,0);
      }
      array_free(np->channels);
      array_init(np->channels,sizeof(channel *));
    } else {
      /* It's an actual channel join */
      newchan=0;
      if ((cp=findchannel(nextchan))==NULL) {
        /* User joined non-existent channel - create it.  Note that createchannel automatically 
         * puts the right magic timestamp in for us */
        Error("channel",ERR_DEBUG,"User %s joined non existent channel %s.",np->nick,nextchan);
        cp=createchannel(nextchan);
        newchan=1;
      }
      if (cp->timestamp==MAGIC_REMOTE_JOIN_TS && timestamp) {
        /* No valid timestamp on the chan and we received one -- set */
        cp->timestamp=timestamp;
      }
      /* OK, this is slightly inefficient since we turn the nick * into a numeric,
       * and addnicktochannel then converts it back again.  BUT it's fewer lines of code :) */
      if (addnicktochannel(cp,np->numeric)) {
        /* The user wasn't added */
        if (newchan) {
          delchannel(cp);
        } 
      } else {
        chanindex *cip=cp->index;
         
        /* If we just created a channel, flag it */
        if (newchan) {
          triggerhook(HOOK_CHANNEL_NEWCHANNEL,cp);
        }
        
        /* Don't send HOOK_CHANNEL_JOIN if the channel doesn't exist any
         * more (can happen if something destroys it in response to
         * HOOK_CHANNEL_NEWCHANNEL) */
        if (cp == cip->channel) {
          /* send hook */
          harg[0]=cp;
          harg[1]=np;
          triggerhook(HOOK_CHANNEL_JOIN,harg);
        }
      }
    }
    nextchan=pos;
  }
  return CMD_OK;
}

int handlecreatemsg(void *source, int cargc, char **cargv) {
  char *pos,*nextchan;
  nick *np;
  channel *cp;
  long timestamp=0;
  int newchan=1;
  void *harg[2];
  
  if (cargc<2) {
    return CMD_OK;
  }
  
  timestamp=strtol(cargv[1],NULL,10);
  
  /* Find out who we are talking about here */
  np=getnickbynumericstr(source);
  if (np==NULL) {
    Error("channel",ERR_WARNING,"Channel create from non existent user %s",source);
    return CMD_OK;  
  }
  
  nextchan=pos=cargv[0];
  while (*nextchan!='\0') {
    /* Find the next chan position */
    for (;*pos!='\0' && *pos!=',';pos++)
      ; /* Empty loop */
  
    if (*pos==',') {
      *pos='\0';
      pos++;
    }
    
    /* OK, pos now points at either null or the next chan 
     * and nextchan now points at the channel name we want to parse next */
     
    /* It's a channel create */
    if ((cp=findchannel(nextchan))==NULL) {
      /* This is the expected case -- the channel didn't exist before */
      cp=createchannel(nextchan);
      cp->timestamp=timestamp;
      newchan=1;
    } else {
      Error("channel",ERR_DEBUG,"Received CREATE for already existing channel %s",cp->index->name->content);
      if (cp->timestamp==MAGIC_REMOTE_JOIN_TS && timestamp) {
        /* No valid timestamp on the chan and we received one -- set */
        cp->timestamp=timestamp;
      }
      newchan=0;
    }
    /* Add the user to the channel, preopped */
    if (addnicktochannel(cp,(np->numeric)|CUMODE_OP)) {
      if (newchan) {
        delchannel(cp);
      }
    } else {
      chanindex *cip = cp->index;
        
      /* Flag the channel as new if necessary */
      if (newchan) {
        triggerhook(HOOK_CHANNEL_NEWCHANNEL,cp);
      }
    
      /* If HOOK_CHANNEL_NEWCHANNEL has caused the channel to be deleted,
       * don't trigger the CREATE hook. */
      if (cip->channel == cp) {
        /* Trigger hook */
        harg[0]=cp;
        harg[1]=np;
        triggerhook(HOOK_CHANNEL_CREATE,harg);
      }
    }
    nextchan=pos;
  }

  return CMD_OK;
}

int handlepartmsg(void *source, int cargc, char **cargv) {
  char *pos,*nextchan;
  nick *np;
  channel *cp;
  void *harg[3];
  
  if (cargc<1) {
    return CMD_OK;
  }
 
  if (cargc>2) {
    Error("channel",ERR_WARNING,"PART with too many parameters (%d)",cargc);
  }
  
  if (cargc>1) {
    harg[2]=(void *)cargv[1];
  } else {
    harg[2]=(void *)"";
  }
  
  /* Find out who we are talking about here */
  np=getnickbynumericstr(source);
  if (np==NULL) {
    Error("channel",ERR_WARNING,"PART from non existent numeric %s",source);
    return CMD_OK;  
  }
  
  harg[1]=np;
  
  nextchan=pos=cargv[0];
  while (*nextchan!='\0') {
    /* Find the next chan position */
    for (;*pos!='\0' && *pos!=',';pos++)
      ; /* Empty loop */
  
    if (*pos==',') {
      *pos='\0';
      pos++;
    }
    
    /* OK, pos now points at either null or the next chan 
     * and nextchan now points at the channel name we want to parse next */
     
    if ((cp=findchannel(nextchan))==NULL) {
      /* Erm, parting a channel that's not there?? */
      Error("channel",ERR_WARNING,"Nick %s left non-existent channel %s",np->nick,nextchan);
    } else {
      /* Trigger hook *FIRST* */
      harg[0]=cp;
      triggerhook(HOOK_CHANNEL_PART,harg);
      
      delnickfromchannel(cp,np->numeric,1);
    }
    nextchan=pos;
  }

  return CMD_OK;
}

int handlekickmsg(void *source, int cargc, char **cargv) {
  nick *np,*kicker;
  channel *cp;
  void *harg[4];
  
  if (cargc<3) {
    return CMD_OK;
  }
  
  /* Find out who we are talking about here */
  if ((np=getnickbynumericstr(cargv[1]))==NULL) {
    Error("channel",ERR_DEBUG,"Non-existant numeric %s kicked from channel %s",source,cargv[0]);
    return CMD_OK;  
  }

  /* And who did the kicking */
  if (((char *)source)[2]=='\0') {
    /* 'Twas a server.. */
    kicker=NULL;
  } else if ((kicker=getnickbynumericstr((char *)source))==NULL) {
    /* It looks strange, but we let the kick go through anyway */
    Error("channel",ERR_DEBUG,"Kick from non-existant nick %s",(char *)source);
  }
  
  /* And find out which channel */
  if ((cp=findchannel(cargv[0]))==NULL) {
    /* OK, not a channel that actually exists then.. */
    Error("channel",ERR_DEBUG,"Nick %s kicked from non-existent channel %s",np->nick,cargv[0]);
  } else {
    /* Before we do anything else, we have to acknowledge the kick to the network */
    if (homeserver(np->numeric)==mylongnum) {
      irc_send("%s L %s",longtonumeric(np->numeric,5),cp->index->name->content);
    }

    /* Trigger hook *FIRST* */
    harg[0]=cp;
    harg[1]=np;
    harg[2]=kicker;
    harg[3]=cargv[2];
    triggerhook(HOOK_CHANNEL_KICK,harg);

    /* We let delnickfromchannel() worry about whether this nick is actually on this channel */
    delnickfromchannel(cp,np->numeric,1);  
  }
  
  return CMD_OK;
}

int handletopicmsg(void *source, int cargc, char **cargv) {
  channel *cp;
  nick *np;
  void *harg[2];
  time_t topictime=0, timestamp=0;
  
  if (cargc<2) {
    return CMD_OK;
  }
  
  if (cargc>2)
    topictime=strtol(cargv[cargc-2], NULL, 10);
  
  if (cargc>3)
    timestamp=strtol(cargv[cargc-3], NULL, 10);
  
  if ((np=getnickbynumericstr((char *)source))==NULL) {
    /* We should check the sender exists, but we still change the topic even if it doesn't */
    Error("channel",ERR_WARNING,"Topic change by non-existent user %s",(char *)source);
  }
  
  /* Grab channel pointer */
  if ((cp=findchannel(cargv[0]))==NULL) {
    /* We're not going to create a channel for the sake of a topic.. */
    return CMD_OK;
  } else {
    if (timestamp && (cp->timestamp < timestamp)) {
      /* Ignore topic change for younger channel 
       * (note that topic change for OLDER channel should be impossible!) */
      return CMD_OK;
    }
    if (topictime && (cp->topictime > topictime)) {
      /* Ignore topic change with older topic */
      return CMD_OK;
    }
    if (cp->topic!=NULL) {
      freesstring(cp->topic);
    }
    if (cargv[cargc-1][0]=='\0') {
      cp->topic=NULL;
      cp->topictime=0;
    } else {
      cp->topic=getsstring(cargv[cargc-1],TOPICLEN);
      cp->topictime=topictime?topictime:getnettime();
    }
    /* Trigger hook */
    harg[0]=cp;
    harg[1]=np;
    triggerhook(HOOK_CHANNEL_TOPIC,harg);
  }
  
  return CMD_OK;
}

/*
 * Note that this function is also used for processing OPMODE messages.
 * There is no checking on the source etc. anyway, so this should be OK.
 * (we are trusting ircd not to feed us bogus modes)
 */

int handlemodemsg(void *source, int cargc, char **cargv) {
  channel *cp;
  int dir=1;
  int arg=2;
  char *modestr;
  unsigned long *lp;
  void *harg[3];
  nick *np, *target;
  int hooknum;
  int changes=0;
  
  if (cargc<2) {
    return CMD_OK;
  }

  if (cargv[0][0]!='#' && cargv[0][0]!='+') {
    /* Not a channel, ignore */
    return CMD_OK;
  }

  if ((cp=findchannel(cargv[0]))==NULL) {
    /* No channel, abort */
    Error("channel",ERR_WARNING,"Mode change on non-existent channel %s",cargv[0]);
    return CMD_OK;
  }
  
  if (((char *)source)[2]=='\0') {
    /* Server mode change, treat as divine intervention */
    np=NULL;
  } else if ((np=getnickbynumericstr((char *)source))==NULL) {
    /* No sender, continue but moan */
    Error("channel",ERR_WARNING,"Mode change by non-existent user %s on channel %s",(char *)source,cp->index->name->content);
  }
  
  /* Set up the hook data */
  harg[0]=cp;
  harg[1]=np;
  
  /* Process the mode string one character at a time */
  /* Maybe I'll write this more intelligently one day if I can comprehend the ircu code that does this */
  for (modestr=cargv[1];*modestr;modestr++) {
    switch(*modestr) {
      /* Set whether we are adding or removing modes */
      
      case '+': 
        dir=1;
        break;
        
      case '-':
        dir=0;
        break;
      
      /* Simple modes: just set or clear based on value of dir */
      
      case 'n':
        if (dir) { SetNoExtMsg(cp); } else { ClearNoExtMsg(cp); }
	changes |= MODECHANGE_MODES;
        break;
      
      case 't':
        if (dir) { SetTopicLimit(cp); } else { ClearTopicLimit(cp); }
	changes |= MODECHANGE_MODES;
        break;
      
      case 's':
        if (dir) { SetSecret(cp); ClearPrivate(cp); } else { ClearSecret(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'p':
        if (dir) { SetPrivate(cp); ClearSecret(cp); } else { ClearPrivate(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'i':
        if (dir) { SetInviteOnly(cp); } else { ClearInviteOnly(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'm':
        if (dir) { SetModerated(cp); } else { ClearModerated(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'c':
        if (dir) { SetNoColour(cp); } else { ClearNoColour(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'C':
        if (dir) { SetNoCTCP(cp); } else { ClearNoCTCP(cp); }
	changes |= MODECHANGE_MODES;
        break;
      
      case 'r':
        if (dir) { SetRegOnly(cp); } else { ClearRegOnly(cp); }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'D':
        if (dir) { SetDelJoins(cp); } else { ClearDelJoins(cp); }
	changes |= MODECHANGE_MODES;
        break;

      case 'u':
        if (dir) { SetNoQuitMsg(cp); } else { ClearNoQuitMsg(cp); }
        changes |= MODECHANGE_MODES;
        break;
      
      case 'N':
        if (dir) { SetNoNotice(cp); } else { ClearNoNotice(cp); }
        changes |= MODECHANGE_MODES;
        break;
        
      case 'M':
        if (dir) { SetModNoAuth(cp); } else { ClearModNoAuth(cp); }
        changes |= MODECHANGE_MODES;
        break;
      
      case 'T':
        if (dir) { SetSingleTarg(cp); } else { ClearSingleTarg(cp); }
        changes |= MODECHANGE_MODES;
        break;
        
      /* Parameter modes: advance parameter and possibly read it in */    
    
      case 'l':
        if (dir) {
          /* +l uses a parameter, but -l does not.
           * If there is no parameter, don't set the mode.
           * I guess we should moan too in that case, but 
           * they might be even nastier to us if we do ;) */
          if (arg<cargc) {
            cp->limit=strtol(cargv[arg++],NULL,10);
            SetLimit(cp);
          }      
        } else {
          ClearLimit(cp);
          cp->limit=0;
        }
	changes |= MODECHANGE_MODES;
        break;
        
      case 'k':
        if (dir) {
          /* +k uses a parameter in both directions */
          if (arg<cargc) {
            freesstring(cp->key); /* It's probably NULL, but be safe */
            cp->key=getsstring(cargv[arg++],KEYLEN);
            SetKey(cp);
          }
        } else {
          freesstring(cp->key);
          cp->key=NULL;
          ClearKey(cp);
          arg++; /* Eat the arg without looking at it, even if it's not there */
        }
	changes |= MODECHANGE_MODES;
        break;
        
      /* Op/Voice */
      
      case 'o':
      case 'v': 
        if (arg<cargc) {
          if((lp=getnumerichandlefromchanhash(cp->users,numerictolong(cargv[arg++],5)))==NULL) {
            /* They're not on the channel; MODE crossed with part/kill/kick/blah */
            Error("channel",ERR_DEBUG,"Mode change for user %s not on channel %s",cargv[arg-1],cp->index->name->content);
          } else { 
            if ((target=getnickbynumeric(*lp))==NULL) {
              /* This really is a fuckup, we found them on the channel but there isn't a user with that numeric */
              /* This means there's a serious bug in the nick/channel tracking code */
              Error("channel",ERR_ERROR,"Mode change for user %s on channel %s who doesn't exist",cargv[arg-1],cp->index->name->content);
            } else { /* Do the mode change whilst admiring the beautiful code layout */
              harg[2]=target;
              if (*modestr=='o') { if (dir) { *lp |= CUMODE_OP;     hooknum=HOOK_CHANNEL_OPPED;    } else 
                                            { *lp &= ~CUMODE_OP;    hooknum=HOOK_CHANNEL_DEOPPED;  } }
                            else { if (dir) { *lp |= CUMODE_VOICE;  hooknum=HOOK_CHANNEL_VOICED;   } else 
                                            { *lp &= ~CUMODE_VOICE; hooknum=HOOK_CHANNEL_DEVOICED; } } 
              triggerhook(hooknum,harg);
            }
          }
        }
	changes |= MODECHANGE_USERS;
        break;
        
      case 'b':
        if (arg<cargc) {
          if (dir) {
            setban(cp,cargv[arg++]);
            triggerhook(HOOK_CHANNEL_BANSET,harg);
          } else {
            clearban(cp,cargv[arg++],0);
            triggerhook(HOOK_CHANNEL_BANCLEAR,harg);
          }
        }
	changes |= MODECHANGE_BANS;
        break;
        
      default:
        Error("channel",ERR_DEBUG,"Unknown mode char '%c' %s on %s",*modestr,dir?"set":"cleared",cp->index->name->content);
        break;
    }
  }

  harg[2]=(void *)((long)changes);
  triggerhook(HOOK_CHANNEL_MODECHANGE,(void *)harg);  
  return CMD_OK;
}

/* 
 * Deal with 2.10.11ism: CLEARMODE
 *
 * [hAAA CM #twilightzone ovpsmikbl
 */

int handleclearmodemsg(void *source, int cargc, char **cargv) {
  channel *cp;
  void *harg[3];
  nick *np, *target;
  char *mcp;
  unsigned long usermask=0;
  int i;
  int changes=0;

  if (cargc<2) {
    return CMD_OK;
  }

  if ((cp=findchannel(cargv[0]))==NULL) {
    /* No channel, abort */
    Error("channel",ERR_WARNING,"Mode change on non-existent channel %s",cargv[0]);
    return CMD_OK;
  }

  if (((char *)source)[2]=='\0') {
    /* Server mode change? (I don't think servers are allowed to do CLEARMODE) */
    np=NULL;
  } else if ((np=getnickbynumericstr((char *)source))==NULL) {
    /* No sender, continue but moan */
    Error("channel",ERR_WARNING,"Mode change by non-existent user %s on channel %s",(char *)source,cp->index->name->content);
  }
             
  harg[0]=cp;
  harg[1]=np;
  
  for (mcp=cargv[1];*mcp;mcp++) {
    switch (*mcp) {
      case 'o':
        usermask |= CUMODE_OP;
	changes |= MODECHANGE_USERS;
        break;
      
      case 'v':
        usermask |= CUMODE_VOICE;
	changes |= MODECHANGE_USERS;
        break;
        
      case 'n':
        ClearNoExtMsg(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 't':
        ClearTopicLimit(cp);
	changes |= MODECHANGE_MODES;
        break;
      
      case 's':
        ClearSecret(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'p':
        ClearPrivate(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'i':
        ClearInviteOnly(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'm':
        ClearModerated(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'c':
        ClearNoColour(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'C':
        ClearNoCTCP(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'r':
        ClearRegOnly(cp);
	changes |= MODECHANGE_MODES;
        break;
      
      case 'D':
        ClearDelJoins(cp);
	changes |= MODECHANGE_MODES;
        break;

      case 'u':
        ClearNoQuitMsg(cp);
        changes |= MODECHANGE_MODES;
	break;

      case 'N':
        ClearNoNotice(cp);
        changes |= MODECHANGE_MODES;
        break;
        
      case 'M':
        ClearModNoAuth(cp);
        changes |= MODECHANGE_MODES;
        break;
      
      case 'T':
        ClearSingleTarg(cp);
        changes |= MODECHANGE_MODES;
        break;

      case 'b':
        clearallbans(cp);
	changes |= MODECHANGE_BANS;
	break;
        
      case 'k':
        /* This is all safe even if there is no key atm */
        freesstring(cp->key);
        cp->key=NULL;
        ClearKey(cp);
	changes |= MODECHANGE_MODES;
        break;
        
      case 'l':
        cp->limit=0;
        ClearLimit(cp);
	changes |= MODECHANGE_MODES;
        break;
    }
  }
   
  if (usermask) {
    /* We have to strip something off each user */
    for (i=0;i<cp->users->hashsize;i++) {
      if (cp->users->content[i]!=nouser && (cp->users->content[i] & usermask)) {
        /* This user exists and has at least one of the modes we're clearing */
        if ((target=getnickbynumeric(cp->users->content[i]))==NULL) {
          /* This really is a fuckup, we found them on the channel but there isn't a user with that numeric */
          /* This means there's a serious bug in the nick/channel tracking code */
          Error("channel",ERR_ERROR,"CLEARMODE failed: user on channel who doesn't exist?");
        } else {
          harg[2]=target;                                  
          /* Yes, these are deliberate three way bitwise ANDs.. */
          if (cp->users->content[i] & usermask & CUMODE_OP)
            triggerhook(HOOK_CHANNEL_DEOPPED, harg);
          if (cp->users->content[i] & usermask & CUMODE_VOICE)
            triggerhook(HOOK_CHANNEL_DEVOICED, harg);          
          cp->users->content[i] &= ~usermask;
        }
      }
    }
  }

  harg[2]=(void *)((long)changes);
  triggerhook(HOOK_CHANNEL_MODECHANGE, harg);
  return CMD_OK;
}

void handlewhoischannels(int hooknum, void *arg) {
  channel **chans;
  char buffer[1024];
  unsigned int bufpos;
  sstring *name;
  unsigned long *num;
  int i;
  void **args = (void **)arg;
  nick *sender = (nick *)args[0], *target = (nick *)args[1];

  if(IsService(target) || IsHideChan(target))
    return;

  chans = (channel **)(target->channels->content);

  buffer[0] = '\0';
  bufpos=0;
  
  /* Not handling delayed joins. */
  for(i=target->channels->cursi-1;i>=0;i--) {
    /* Secret / Private channels: only show if the sender is on the channel as well */
    if(IsSecret(chans[i]) || IsPrivate(chans[i])) {
      if (!getnumerichandlefromchanhash(chans[i]->users, sender->numeric))
        continue;
    }

    name = chans[i]->index->name;
    if (bufpos + name->length > 508) { /* why 508? - need room for -@#channame\0 + 1 slack */
      irc_send("%s", buffer);
      buffer[0] = '\0';
      bufpos=0;
    }

    if(buffer[0] == '\0')
      bufpos=snprintf(buffer, sizeof(buffer), ":%s 319 %s %s :", myserver->content, sender->nick, target->nick);

    num = getnumerichandlefromchanhash(chans[i]->users, target->numeric);

    /* Adding these flags might make the string "unsafe" (without terminating \0). */
    /* sprintf'ing the channel name afterwards is guaranteed to fix it though */
    if (IsDeaf(target))
      buffer[bufpos++]='-';
    if (*num & CUMODE_OP)
      buffer[bufpos++]='@';
    else if (*num & CUMODE_VOICE)
      buffer[bufpos++]='+';

    bufpos += sprintf(buffer+bufpos, "%s ",name->content);
  }

  if (buffer[0] != '\0')
    irc_send("%s", buffer);
}

