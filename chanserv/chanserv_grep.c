
#include "chanserv.h"
#include "../core/events.h"
#include "../lib/irc_string.h"
#include <pcre.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "../lib/version.h"

MODULE_VERSION(QVERSION)

#define CSG_BUFSIZE    1024
#define CSG_MAXSTARTPOINT    30

pcre           *csg_curpat;        /* Compiled pattern from pcre */
int             csg_curfile;       /* Which logfile is being searched */
unsigned long   csg_curnum;        /* What numeric is doing a search */
int             csg_matches;       /* How many lines have been returned so far */
int             csg_maxmatches=0;  /* How many matches are allowed */
int		csg_direction;     /* Log direction (0 = forward, 1 = reverse) */
int		csg_bytesread;

char            csg_readbuf[CSG_BUFSIZE];  /* Buffer */
int             csg_bytesleft;     /* How much valid data there is in the buffer */

void csg_handleevents(int fd, short revents);
int csg_dogrep(void *source, int cargc, char **cargv);
int csg_dorgrep(void *source, int cargc, char **cargv);
int csg_execgrep(nick *sender, char *pattern);

#if !defined(pread)
extern ssize_t pread(int fd, void *buf, size_t count, off_t offset);
#endif

void _init() {
  chanservaddcommand("grep",   QCMD_OPER, 1, csg_dogrep,   "Searches the logs.","Usage: GREP <regex>\nSearches the logs.  The current logfile will be specified first, followed by\nall older logfiles found.  This will shuffle the order of results slightly.  Where:\nregex  - regular expression to search for.\nNote: For a case insensitive search, prepend (?i) to the regex.");
  chanservaddcommand("rgrep",  QCMD_OPER, 2, csg_dorgrep,  "Searches the logs in reverse order.","Usage: RGREP <days> <regex>\nSearches the logs.  The oldest specified log will be specified first meaning\nthat all events returned will be in strict chronological order. Where:\ndays  - number of days of history to search\nregex - regex to search for\nNote: For a case insensitive search, prepend (?i) to the regex.");
}

void _fini() {
  chanservremovecommand("grep", csg_dogrep);
  chanservremovecommand("rgrep", csg_dorgrep);
}

int csg_dogrep(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);

  if (!rup)
    return CMD_ERROR;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "grep");
    return CMD_ERROR;
  }

  csg_curfile=0;
  csg_direction=0;

  chanservwallmessage("%s (%s) used GREP %s", sender->nick, rup->username, cargv[0]);
  cs_log(sender, "GREP %s", cargv[0]);

  return csg_execgrep(sender, cargv[0]);
}

int csg_dorgrep(void *source, int cargc, char **cargv) {
  int startpoint;
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);

  if (!rup)
    return CMD_ERROR;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "rgrep");
    return CMD_ERROR;
  }

  if (!protectedatoi(cargv[0], &startpoint)) {
    chanservsendmessage(sender, "Error in starting day number.");
    return CMD_ERROR;
  }

  if (startpoint<0) {
    chanservsendmessage(sender, "Invalid starting day number.");
    return CMD_ERROR;
  }

  if (startpoint>CSG_MAXSTARTPOINT) {
    chanservsendmessage(sender, "Sorry, the maximum starting day is %d days.", CSG_MAXSTARTPOINT);
    return CMD_ERROR;
  }

  chanservwallmessage("%s (%s) used RGREP %s %s", sender->nick, rup->username, cargv[0], cargv[1]);
  cs_log(sender, "RGREP %s %s", cargv[0], cargv[1]);

  csg_curfile=startpoint;
  csg_direction=1;
  return csg_execgrep(sender, cargv[1]);
}

int csg_execgrep(nick *sender, char *pattern) {
  const char *errptr;
  int erroffset;
  int fd;
  char filename[50];

  if (csg_maxmatches>0) {
    chanservsendmessage(sender, "Sorry, the grepper is currently busy - try later.");
    return CMD_ERROR;
  }

  if (!(csg_curpat=pcre_compile(pattern, 0, &errptr, &erroffset, NULL))) {
    chanservsendmessage(sender, "Error in pattern at character %d: %s",erroffset,errptr);
    return CMD_ERROR;
  }

  if (csg_direction==0 || csg_curfile==0) {
    if ((fd=open("chanservlog",O_RDONLY))<0) {
      chanservsendmessage(sender, "Unable to open logfile.");
      free(csg_curpat);    
      return CMD_ERROR;
    }
  } else {
    sprintf(filename,"chanservlog.%d",csg_curfile);
    if ((fd=open(filename,O_RDONLY))<0) {
      chanservsendmessage(sender, "Unable to open logfile.");
      free(csg_curpat);    
      return CMD_ERROR;
    }
  }

  /* Initialise stuff for the match */
  csg_maxmatches=500;
  csg_matches=0;
  csg_curnum=sender->numeric;
  csg_bytesleft=0;
  csg_bytesread=0;

  registerhandler(fd, POLLIN, csg_handleevents);
  chanservsendmessage(sender, "Started grep for %s...",pattern);

  return CMD_OK;
}

void csg_handleevents(int fd, short revents) {
  int res;
  nick *np=getnickbynumeric(csg_curnum);
  char *chp, *linestart;
  char filename[50];
  off_t pos;

  /* If the target user has vanished, drop everything */
  if (!np) {
    deregisterhandler(fd, 1);
    free(csg_curpat);
    csg_maxmatches=0;
    return;
  }

retry:
  res=read(fd, csg_readbuf+csg_bytesleft, CSG_BUFSIZE-csg_bytesleft);
  
  if (res<=0) {
/*    chanservsendmessage(np, "Closing file: res=%d, errno=%d(%s), bytes read=%d",res,errno,sys_errlist[errno],csg_bytesread); */
    /* End of file (or error) */
    deregisterhandler(fd, 1);
    if (csg_direction==0) {
      sprintf(filename,"chanservlog.%d",++csg_curfile);
    } else {
      csg_curfile--;
      if (csg_curfile<0) {
        chanservstdmessage(np, QM_ENDOFLIST);
        free(csg_curpat);
        csg_maxmatches=0;
        return;
      } else if (csg_curfile==0) {
        sprintf(filename,"chanservlog");
      } else {
        sprintf(filename,"chanservlog.%d",csg_curfile);
      }
    }
    if ((fd=open(filename,O_RDONLY))>=0) {
      /* Found the next file */
      registerhandler(fd, POLLIN, csg_handleevents);
    } else {
      chanservstdmessage(np, QM_ENDOFLIST);
      free(csg_curpat);
      csg_maxmatches=0;
    }

    return;
  }

  csg_bytesread+=res;

  linestart=chp=csg_readbuf;
  csg_bytesleft+=res;
  
  while (csg_bytesleft) {
    csg_bytesleft--;
    if (*chp=='\r' || *chp=='\n' || *chp=='\0') {
      if (linestart==chp) {
	linestart=(++chp);
      } else {
	*chp++='\0';
	if (!pcre_exec(csg_curpat, NULL, linestart, strlen(linestart), 0, 0, NULL, 0)) {
	  chanservsendmessage(np, "%s", linestart);
	  if (++csg_matches >= csg_maxmatches) {
	    chanservstdmessage(np, QM_TRUNCATED, csg_maxmatches);
	    chanservstdmessage(np, QM_ENDOFLIST);
	    free(csg_curpat);
	    deregisterhandler(fd, 1);
	    csg_maxmatches=0;
	  } 
	}
	linestart=chp;
      }
    } else {
      chp++;
    }
  }
  
  csg_bytesleft=(chp-linestart);
  memmove(csg_readbuf, linestart, csg_bytesleft);
  
  /* BSD hack */
  pos=lseek(fd, 0, SEEK_CUR);
  if (!pread(fd, csg_readbuf+csg_bytesleft, 1, pos)) {
    /* EOF */
    goto retry;
  }    
}
