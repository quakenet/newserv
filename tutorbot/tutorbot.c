#include <stdio.h>
#include <string.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"

MODULE_VERSION("");

#include <stdio.h>

#define STAFFCHAN    "#tutorial.staff"
#define QUESTIONCHAN "#tutorial.questions"
#define PUBLICCHAN   "#tutorial"

#define QUESTIONINTERVAL 30

#define SPAMINTERVAL 10

#define NICKMONITORTIME  300

#define QUESTIONHASHSIZE 1000

#define QUESTION_NEW       0x0
#define QUESTION_ANSWERED  0x1
#define QUESTION_OFFTOPIC  0x2
#define QUESTION_SPAM      0x3

#define QUESTION_STATE     0x7

typedef struct spammsg {
  sstring         *message;
  struct spammsg  *next;
} spammsg;

typedef struct questrec {
  int              ID;
  sstring         *question;
  flag_t           flags;
  char             nick[NICKLEN+1];
  unsigned long    numeric;
  sstring         *answer;
  struct questrec *next;
} questrec;

typedef struct blockeduser {
  int              type; /* 0 = account, 1 = hostmask */
  char             content[NICKLEN+USERLEN+HOSTLEN+3]; /* now includes a mask or an accountname */
  struct blockeduser *next;
} blockeduser;

typedef struct storedanswer {
  questrec         *qptr;
  char             nick[NICKLEN+1];
  struct storedanswer *next;
} storedanswer;
  

nick *tutornick;
int tutornext;
unsigned int micnumeric; /* Who has the mic atm */
spammsg  *nextspam;
spammsg  *lastspam;
time_t    spamtime;
int       lastquestionID;
questrec *questiontable[QUESTIONHASHSIZE];
blockeduser *blockedusers;
storedanswer *storedanswers;

void spamstored();
void tutorhandler(nick *me, int type, void **args);

void tutorspam(void *arg) {
  channel *cp;
  spammsg *smp;

  if (tutornick && (cp=findchannel(PUBLICCHAN))) {
    sendmessagetochannel(tutornick, cp, "%s", nextspam->message->content);
  }
  
  smp=nextspam;
  nextspam=nextspam->next;
  
  freesstring(smp->message);
  free(smp);

  spamtime=time(NULL);

  if (nextspam) {
    scheduleoneshot(spamtime+SPAMINTERVAL, tutorspam, NULL);
  } else {
    lastspam=NULL;
    if (!micnumeric) 
      spamstored(); 
  }
}

void spamstored(void) {
  storedanswer *nsa;
  channel *cp;

  cp=findchannel(PUBLICCHAN);  
  
  if (!cp || !tutornick) 
    return;
    
  while(storedanswers) {
    nsa=storedanswers;  
    sendmessagetochannel(tutornick,cp,"%s asked: %s",storedanswers->qptr->nick,storedanswers->qptr->question->content);
    sendmessagetochannel(tutornick,cp,"%s answered: %s",storedanswers->nick,storedanswers->qptr->answer->content);
    
    storedanswers=storedanswers->next;
    free(nsa);  
  }
}

void usercountspam(void *arg) {
  channel *cp, *cp2;
  
  if (!(cp=findchannel(PUBLICCHAN)))
    return;
    
  if (!(cp2=findchannel(STAFFCHAN)))
    return;
  
  if (!tutornick)
    return;
    
  sendmessagetochannel(tutornick, cp2, "Currently %d users in %s.",cp->users->totalusers,cp->index->name->content);
}

void flushspamqueue() {
  spammsg *smp,*nsmp;
  
  for (smp=nextspam;smp;smp=nsmp) {
    nsmp=smp->next;
    freesstring(smp->message);
    free(smp);
  }

  deleteallschedules(tutorspam);
  nextspam=lastspam=NULL;
}

void _init() {
  channel *cp;

  if ((tutornext=registernickext("tutorbot")) > -1) {
    micnumeric=0;
    nextspam=lastspam=NULL;
    blockedusers=NULL;
    storedanswers=NULL;
    spamtime=0;
    lastquestionID=0;
    memset(questiontable, 0, sizeof(questrec *) * QUESTIONHASHSIZE);
    tutornick=registerlocaluser("Tutor","tutor","quakenet.org","#tutorial bot","tutor",UMODE_ACCOUNT | UMODE_INV,
				tutorhandler);
    
    if ((cp=findchannel(PUBLICCHAN))) {
      localjoinchannel(tutornick,cp);
      localgetops(tutornick, cp);
    } else {
      localcreatechannel(tutornick,PUBLICCHAN);
    }

    if ((cp=findchannel(QUESTIONCHAN))) {
      localjoinchannel(tutornick,cp);
    } else {
      localcreatechannel(tutornick,QUESTIONCHAN);
    }

    if ((cp=findchannel(STAFFCHAN))) {
      localjoinchannel(tutornick,cp);
    } else {
      localcreatechannel(tutornick,STAFFCHAN);
    }
    
    schedulerecurring(time(NULL)+300, 0, 300, usercountspam, NULL);
  } else {
    Error("tutorbot",ERR_ERROR,"Unable to get nickext");
  }
}
 
void _fini() {
  int i;
  questrec *qrp, *nqrp;
  blockeduser *bup;
  
  if (tutornext > -1) {
    releasenickext(tutornext);
    deregisterlocaluser(tutornick,NULL);
    deleteallschedules(usercountspam);
    flushspamqueue();
    
    /* remove all blockedusers */
    while (blockedusers!=NULL) {
      bup=blockedusers;
      blockedusers=blockedusers->next;
      free(bup);
    }

    /* Blow away all stored questions */
    for(i=0;i<QUESTIONHASHSIZE;i++) {
      for (qrp=questiontable[i];qrp;qrp=nqrp) {
        nqrp=qrp->next;
        freesstring(qrp->question);
        freesstring(qrp->answer);
        free(qrp);
      }
    }
  }    
}

void tutordump() {
  FILE *fp;
  int i;
  questrec *qrp;
  int j;
  
  if (!(fp=fopen("tutor-questions","w")))
    return;

  for (i=0;i<4;i++) {
    switch(i) {
      case QUESTION_NEW:
        fprintf(fp,"Untouched questions:\n\n");
        break;
        
      case QUESTION_ANSWERED:
        fprintf(fp,"\nAnswered questions:\n\n");
        break;
        
      case QUESTION_OFFTOPIC:
        fprintf(fp,"\nOff-topic questions:\n\n");
        break;
        
      case QUESTION_SPAM:
        fprintf(fp,"\nSpam:\n\n");
        break;
    }
    
    for (j=0;j<=lastquestionID;j++) {
      for (qrp=questiontable[j%QUESTIONHASHSIZE];qrp;qrp=qrp->next)
        if (qrp->ID==j)
          break;
          
      if (!qrp)
        continue;
        
      if ((qrp->flags & QUESTION_STATE) == i) {
        fprintf(fp,"(%s) %s\n",qrp->nick,qrp->question->content);
        
        if (i==QUESTION_ANSWERED)
          fprintf(fp,"Answer: %s\n\n",qrp->answer->content);
      }
    }
  }
  
  fclose(fp);
}
    
void tutorhandler(nick *me, int type, void **args) {
  nick *np;
  channel *cp;
  char *text;
  spammsg *smp;
  time_t lastmsg;

  if (type==LU_CHANMSG) {
    np=args[0];
    cp=args[1];
    text=args[2];
    
    /* Staff channel *only* commands */
    if (!ircd_strcmp(cp->index->name->content,STAFFCHAN)) {
      if (!ircd_strncmp(text,"!mic",4)) {
	if (micnumeric) {
	  if (micnumeric==np->numeric) {
	    micnumeric=0;
	    sendmessagetochannel(me,cp,"Mic deactivated.");
            if (!lastspam) 
              spamstored();
                        
	  } else {
	    micnumeric=np->numeric;
	    sendmessagetochannel(me,cp,"%s now has the mic.  Anything said by %s will be relayed in %s.",
				 np->nick, np->nick, PUBLICCHAN);
	  }
	} else {
	  micnumeric=np->numeric;
	  sendmessagetochannel(me,cp,"Mic activated.  Anything said by %s will be relayed in %s.",
			       np->nick, PUBLICCHAN);
	}
      } else if (!ircd_strncmp(text,"!m00",4)) {
        /* mandatory m00 command */
        int i,maxm00=rand()%100; 
        char m00[101];
        
        for(i=0;i<maxm00;i++) {
          int mt=rand()%3;
          if (mt==0) {
            m00[i]='o';
          } else if (mt==1) {
            m00[i]='0';
          } else {
            m00[i]='O';
          }
        }
        m00[i]='\0';
        
        sendmessagetochannel(me,cp,"m%s",m00);
      } else if (!ircd_strncmp(text,"!clear",6)) {
	flushspamqueue();
	sendmessagetochannel(me,cp,"Cleared message buffer.");
	if (micnumeric) {
	  micnumeric=0;
	  sendmessagetochannel(me,cp,"Mic deactivated.");
	}
      } else if (!ircd_strncmp(text,"!save",5)) {
        tutordump();
        sendnoticetouser(me,np,"Save done.");      
      } else if (*text != '!' && np->numeric==micnumeric) {
	/* Message to be relayed */
	if (lastspam) {
	  smp=(spammsg *)malloc(sizeof(spammsg));
	  smp->message=getsstring(text, 500);
	  smp->next=NULL;
	  lastspam->next=smp;
	  lastspam=smp;
	} else {
	  if (spamtime + SPAMINTERVAL < time(NULL)) {
	    /* Spam it directly */
	    if ((cp=findchannel(PUBLICCHAN))) {
	      sendmessagetochannel(me, cp, "%s", text);
	    }
	    spamtime=time(NULL);
	  } else {
	    /* Queue it and start the clock */
	    smp=(spammsg *)malloc(sizeof(spammsg));
	    smp->message=getsstring(text, 500);
	    smp->next=NULL;
	    lastspam=nextspam=smp;
	    scheduleoneshot(spamtime+SPAMINTERVAL, tutorspam, NULL);
	  }
	}
      }
    }
    
    /* Commands for staff channel or question channel */    
    if (!ircd_strcmp(cp->index->name->content,STAFFCHAN) || !ircd_strcmp(cp->index->name->content,QUESTIONCHAN)) {
      if (!ircd_strncmp(text, "!spam ", 6)) {
        int qid;
        char *ch=text+5;
        questrec *qrp;
        
        while (*ch) {
          while (*(++ch) == ' '); /* skip spaces */
          
          qid=strtol(ch, &ch, 10);

          if (qid < 1 || qid > lastquestionID) {
            sendnoticetouser(me,np,"Invalid question ID %d.",qid);
            continue;
          }
        
          for (qrp=questiontable[qid%QUESTIONHASHSIZE];qrp;qrp=qrp->next)
            if (qrp->ID==qid)
              break;         
            
          if (!qrp) {
            sendnoticetouser(me,np,"Can't find question %d.",qid);
            continue;
          }

          qrp->flags = ((qrp->flags) & ~QUESTION_STATE) | QUESTION_SPAM;
          sendnoticetouser(me,np,"Qustion %d has been marked as spam.",qid);
        }
      } else if (!ircd_strncmp(text, "!offtopic ", 10)) {
        int qid;
        char *ch=text+9;
        questrec *qrp;
        
        while (*ch) {
          while (*(++ch) == ' '); /* skip spaces */
          
          qid=strtol(ch, &ch, 10);

          if (qid < 1 || qid > lastquestionID) {
            sendnoticetouser(me,np,"Invalid question ID %d.",qid);
            continue;
          }
        
          for (qrp=questiontable[qid%QUESTIONHASHSIZE];qrp;qrp=qrp->next)
            if (qrp->ID==qid)
              break;         
            
          if (!qrp) {
            sendnoticetouser(me,np,"Can't find question %d.",qid);
            continue;
          }

          qrp->flags = ((qrp->flags) & ~QUESTION_STATE) | QUESTION_OFFTOPIC;
          sendnoticetouser(me,np,"Qustion %d has been marked as off-topic.",qid);
        }
      } else if (!ircd_strncmp(text, "!reset ",7)) {
        if (!ircd_strncmp(text+7,"blocks",6) || !ircd_strncmp(text+7,"all",3)) {
          /* reset blocked users */
          blockeduser *bup;
          
          while(blockedusers) {
            bup=blockedusers;
            blockedusers=blockedusers->next;
	    free(bup);
          }
	  
          sendnoticetouser(me,np,"Reset (blocks): Done.");
          
          /* XXX: Grimless alert: only finish here if user asked for "blocks";
           *      "all" case must drop down to below" */
          if (!ircd_strncmp(text+7,"blocks",6))
            return;
        }
        
        if (!ircd_strncmp(text+7,"questions",9) || !ircd_strncmp(text+7,"all",3)) {
          /* reset questions */
          int i;
          questrec *qrp,*nqrp;
          
          for(i=0;i<QUESTIONHASHSIZE;i++) {
            for(qrp=questiontable[i];qrp;qrp=nqrp) {              
              nqrp=qrp->next;
              
              freesstring(qrp->question);
              freesstring(qrp->answer);
              free(qrp);
            }
            questiontable[i]=NULL;
          }
          
          lastquestionID=0;
          sendnoticetouser(me,np,"Reset (questions): Done.");
          
          return;
        }

        sendnoticetouser(me,np,"Unknown parameter: %s",text+7);
        return;
      } else if (!ircd_strncmp(text, "!reset",6)) {
        sendnoticetouser(me,np,"!reset: more parameters needed.");
        sendnoticetouser(me,np,"        <all|questions|blocks>");
        return;
      } else if (!ircd_strncmp(text, "!listblocks", 11)) {
        blockeduser *bu;
        
        if (!blockedusers) {
          sendnoticetouser(me,np,"There are no blocked users.");
          return;
        }
        
        sendnoticetouser(me,np,"Type  Hostmask/Account");
        
        for(bu=blockedusers;bu;bu=bu->next) {
          if (!bu->type) {
            sendnoticetouser(me,np,"A  %s",bu->content);
          } else {
            sendnoticetouser(me,np,"H  %s",bu->content);
          }
        }
        
        sendnoticetouser(me,np,"End of list.");
        return;
      } else if (!ircd_strncmp(text, "!closechan", 10)) {
        channel *cp;
        modechanges changes;
        if (!(cp=findchannel(PUBLICCHAN))) { 
          sendnoticetouser(me,np,"Can't find public channel!");
          return;
        }
        localsetmodeinit(&changes,cp,me);
        localdosetmode_simple(&changes,CHANMODE_INVITEONLY,0);
        localsetmodeflush(&changes,1);
        sendnoticetouser(me,np,"Public channel has been closed.");
        return;
      } else if (!ircd_strncmp(text, "!openchan", 9)) {
        channel *cp;
        modechanges changes;
        if (!(cp=findchannel(PUBLICCHAN))) {
          sendnoticetouser(me,np,"Can't find public channel!");
          return;
        }
        localsetmodeinit(&changes,cp,me);
        localdosetmode_simple(&changes,CHANMODE_MODERATE|CHANMODE_DELJOINS,CHANMODE_INVITEONLY);
        localsetmodeflush(&changes,1);
        sendnoticetouser(me,np,"Public channel has been opened.");
        return;
      } else if (!ircd_strncmp(text, "!unblock ", 9)) {
        /* remove a block */
        if (!ircd_strncmp(text+9,"-q",2)) {
          /* remove a blocked accountname */
          blockeduser *bu,*bu2=NULL;
          
          if (text[12]=='#') {
            for(bu=blockedusers;bu;bu=bu->next) {
              if (!bu->type && !ircd_strncmp(text+13,bu->content,ACCOUNTLEN)) {
                if (!bu2)
                  blockedusers = bu->next;
                else
                  bu2->next = bu->next;
                  
                free(bu);
                sendnoticetouser(me,np,"Block for users with accountname %s has been removed.",text+13);
                return;
              }
              bu2 = bu;
            }
          } else {
            nick *bun;
            bun=getnickbynick(text+12);
            
            if (!bun) {
              sendnoticetouser(me,np,"Unkown user: %s.",text+12);
              return;
            }

            if (!IsAccount(bun)) {
              sendnoticetouser(me,np,"%s is not authed.",text+12);
              return;
            }

            for(bu=blockedusers;bu;bu=bu->next) {
              if (!bu->type && !ircd_strncmp(text+12,bu->content,ACCOUNTLEN)) {
                if (!bu2)
                  blockedusers=bu->next;
                else 
                  bu2->next=bu->next;
                  
                free(bu);
                sendnoticetouser(me,np,"Block for users with accountname %s has been removed.",text+12);
                return;
              }
              bu2=bu;
            }
          }
           
          sendnoticetouser(me,np,"No such blocked account %s.",text+12);
          return;
        } else {
          /* it's a hostmask, check if the hostmask is valid */
          char *textc=text+9;
          blockeduser *bu,*bu2=NULL;
          
          if (!strchr(textc,'@') || !strchr(textc,'!')) {
            /* not a valid hostmask */
            sendnoticetouser(me,np,"%s is not a valid hostmask.",textc);
            return;
          }
                    
          for(bu=blockedusers;bu;bu=bu->next) {
            if (bu->type && !ircd_strncmp(textc,bu->content,ACCOUNTLEN+USERLEN+NICKLEN+2)) {
              if (!bu2)
                blockedusers = bu->next;
              else 
                bu2->next = bu->next;
                
              free(bu);
              sendnoticetouser(me,np,"Block for users with a hostmask matching %s has been removed.",textc);
              return;
            }
            bu2 = bu;
          }          
              
          sendnoticetouser(me,np,"No such blocked hostmask %s.",textc);
          return;
        }
      } else if (!ircd_strncmp(text, "!block ", 7)) {
        /* Block a user */
        char *nickp;

        if (!ircd_strncmp(text+7,"-q",2)) { 
          /* block the user by his accountname */
          blockeduser *bu;
          nick *bun;
          char *bptr;

          nickp=text+10;
	  if (*nickp=='#') {
	    /* Account given so we will use it */
            bptr=nickp+1;
          } else {            
            bun=getnickbynick(nickp);

            if (!bun) { 
              sendnoticetouser(me,np,"Couldn't find user %s.",nickp); 
              return; 
            }
            
            if (!IsAccount(bun)) { 
              sendnoticetouser(me,np,"%s is not authed.",nickp); 
              return; 
            }
            
            bptr=bun->authname;
          }

          bu=(blockeduser *)malloc(sizeof(blockeduser));
          bu->type=0;
          strncpy(bu->content,bptr,ACCOUNTLEN);
          
          bu->next=blockedusers;
          blockedusers=bu;

          sendnoticetouser(me,np,"Now blocking all messages from users with accountname %s.",bptr);
          return;  
        } else {
          /* block the user by a hostmask */
          char *textc;
          blockeduser *bu;

          textc=text+7;

          if (!strchr(textc,'@') || !strchr(textc,'!')) {
            /* not a valid hostmask */
            sendnoticetouser(me,np,"%s is not a valid hostmask.",textc);
            return;
          }
          
          bu=(blockeduser *)malloc(sizeof(blockeduser));          
          bu->type=1;
          strncpy(bu->content,textc,NICKLEN+USERLEN+HOSTLEN+2);

          bu->next=blockedusers;
          blockedusers=bu;

          sendnoticetouser(me,np,"Now blocking all messages from users with a hostmask matching %s.",textc);
        }
      } else if (!ircd_strncmp(text, "!answer ", 8)) {
        /* Answering a question */
        int qid;
        char *ch=text+7;
        channel *pcp;
        questrec *qrp;
        
        /* answers will no be stored and sent later on if mic is enabled */
                
        while (*(++ch) == ' '); /* skip spaces */
        
        qid=strtol(ch, &ch, 10);

        if (!*ch) {
          sendnoticetouser(me,np,"No answer supplied.");
          return;
        }
                
        while (*(++ch) == ' '); /* skip spaces */
        
        if (!*ch) {
          sendnoticetouser(me,np,"No answer supplied.");
          return;
        }
        
        if (qid < 1 || qid > lastquestionID) {
          sendnoticetouser(me,np,"Invalid question ID %d.",qid);
          return;
        }
        
        for (qrp=questiontable[qid%QUESTIONHASHSIZE];qrp;qrp=qrp->next)
          if (qrp->ID==qid)
            break;
            
        if (!qrp) {
          sendnoticetouser(me,np,"Can't find question %d.",qid);
          return;
        }
        
        switch(qrp->flags & QUESTION_STATE) {
          case QUESTION_ANSWERED:
            sendnoticetouser(me,np,"Question %d has already been answered.",qid);
            return;
          
          case QUESTION_OFFTOPIC:
            sendnoticetouser(me,np,"Question %d has been marked off-topic.",qid);
            return;
            
          case QUESTION_SPAM:
            sendnoticetouser(me,np,"Question %d has been marked as spam.",qid);
            return;
            
          default:
            break;
        }
            
        qrp->flags = ((qrp->flags) & ~QUESTION_STATE) | QUESTION_ANSWERED;
        qrp->answer=getsstring(ch, 250);
       
        if ((pcp=findchannel(PUBLICCHAN)) && (!nextspam) && (!micnumeric)) { 
          sendmessagetochannel(me, pcp, "%s asked: %s",qrp->nick,qrp->question->content);
          sendmessagetochannel(me, pcp, "%s answers: %s",np->nick,ch);
        } else {
          storedanswer *ans;
          ans=malloc(sizeof(storedanswer));
          ans->qptr=qrp;
          strncpy(ans->nick,np->nick,NICKLEN);

          ans->next=storedanswers;
          storedanswers=ans; 

          sendnoticetouser(me,np,"Can't send your answer right now. Answer was stored and will be sent later on.");
          return;
        }        
        
        sendnoticetouser(me,np,"Answer to question %d has been sent and stored.",qid);
      }
    }
  } else if (type==LU_PRIVMSG || type==LU_SECUREMSG) {
    np=args[0];
    text=args[1];
    
    lastmsg=(time_t)np->exts[tutornext];
    
    if (lastmsg + QUESTIONINTERVAL > time(NULL)) {
      sendnoticetouser(me, np, "You have already sent a question recently - please wait at least 30 seconds between asking questions");
    } else {
      char hostbuf[HOSTLEN+USERLEN+NICKLEN+3];
      questrec *qrp; 
      blockeduser *bu;
      channel *cp;
      
      sendnoticetouser(me, np, "Your question has been relayed to the #tutorial staff.");
      
      cp=findchannel(PUBLICCHAN);
      
      if (!getnumerichandlefromchanhash(cp->users, np->numeric))
        return;

      if ((strlen(text)<5) || (!strchr(text,' ')) || (*text==1))
        return;
      
      sprintf(hostbuf,"%s!%s@%s",np->nick,np->ident,np->host->name->content);
      
      for(bu=blockedusers;bu;bu=bu->next) {
        if (bu->type) {
          /* user@host check */
          if (!match(bu->content,hostbuf))
            return;
        } else {
          /* account name check */
          if ((IsAccount(np)) && !ircd_strncmp(np->authname,bu->content,ACCOUNTLEN))
            return;
	}
      }
      
      np->exts[tutornext] = (void *)time(NULL);
      
      qrp=(questrec *)malloc(sizeof(questrec));
      
      qrp->ID=++lastquestionID;
      qrp->question=getsstring(text,250);
      qrp->flags=QUESTION_NEW;
      strcpy(qrp->nick, np->nick);
      qrp->numeric=np->numeric;
      qrp->answer=NULL;
      qrp->next=questiontable[qrp->ID % QUESTIONHASHSIZE];
      
      questiontable[qrp->ID % QUESTIONHASHSIZE] = qrp;
      
      if ((cp=findchannel(QUESTIONCHAN))) {
        sendmessagetochannel(me, cp, "ID: %3d <%s> %s", qrp->ID, visiblehostmask(np, hostbuf), text);
	
        /* Send a seperator every 10 questions */
        if ((lastquestionID % 10)==0)
          sendmessagetochannel(me,cp,"-----------------------------------");
      }
    }
  } else if (type==LU_KILLED) {
    tutornick=NULL;
  } else if (type==LU_INVITE) {
    /* we were invited, check if someone invited us to PUBLICCHAN */
    cp=args[1];
    if (!ircd_strcmp(cp->index->name->content,PUBLICCHAN)) {
      localjoinchannel(me,cp);
      localgetops(me,cp);
    }
    return;
  }
}
