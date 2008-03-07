
#include "chanserv.h"
#include "../core/events.h"
#include <pcre.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define CSG_BUFSIZE    512

pcre           *csg_curpat;        /* Compiled pattern from pcre */
int             csg_curfile;       /* Which logfile is being searched */
unsigned long   csg_curnum;        /* What numeric is doing a search */
int             csg_matches;       /* How many lines have been returned so far */
int             csg_maxmatches=0;  /* How many matches are allowed */

char            csg_readbuf[CSG_BUFSIZE];  /* Buffer */
int             csg_bytesleft;     /* How much valid data there is in the buffer */

void csg_handleevents(int fd, short revents);
int csg_dogrep(void *source, int cargc, char **cargv);

#if !defined(pread)
extern ssize_t pread(int fd, void *buf, size_t count, off_t offset);
#endif

void _init() {
  chanservaddcommand("grep",   QCMD_OPER, 1, csg_dogrep,   "Searches the logs.","");
}

void _fini() {
  chanservremovecommand("grep", csg_dogrep);
}

int csg_dogrep(void *source, int cargc, char **cargv) {
  nick *sender=source;
  const char *errptr;
  int erroffset;
  int fd;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "grep");
    return CMD_ERROR;
  }

  if (csg_maxmatches>0) {
    chanservsendmessage(sender, "Sorry, grep is currently busy - try later.");
    return CMD_ERROR;
  }

  if (!(csg_curpat=pcre_compile(cargv[0], 0, &errptr, &erroffset, NULL))) {
    chanservsendmessage(sender, "Error in pattern at character %d: %s",erroffset,errptr);
    return CMD_ERROR;
  }

  if ((fd=open("chanservlog.0",O_RDONLY))<0) {
    chanservsendmessage(sender, "Unable to open logfile.");
    free(csg_curpat);    
    return CMD_ERROR;
  }

  /* Initialise stuff for the match */
  csg_maxmatches=500;
  csg_matches=0;
  csg_curnum=sender->numeric;
  csg_curfile=0;
  csg_bytesleft=0;

  registerhandler(fd, POLLIN, csg_handleevents);
  chanservsendmessage(sender, "Started grep for %s...",cargv[0]);

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
    /* End of file (or error) */
    deregisterhandler(fd, 1);
    sprintf(filename,"chanservlog.%d",++csg_curfile);
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
