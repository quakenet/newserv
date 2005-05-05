#include "../core/error.h"
#include "../lib/irc_string.h"
#include "proxyscan.h"
#include "../core/events.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#define PSM_NOTCONNECTED 0x00
#define PSM_CONNECTING   0x01
#define PSM_SENTHELLO    0x02
#define PSM_IDLE         0x03
#define PSM_SENTFROM     0x04
#define PSM_SENTTO1      0x05
#define PSM_SENTTO2      0x06
#define PSM_SENTDATA     0x07
#define PSM_SENTBODY     0x08

struct psmail {
  char content[2000];
  struct psmail *next;
};

void ps_flushall();
void ps_mailconnect();
int ps_flushbuf();

const char *psmailtemplate = "From: QuakeNet Proxyscan <proxyscan@quakenet.org>\r\n"
                             "To: <bopm@reports.blitzed.org>\r\n"
                             "Subject: Open proxy report\r\n\r\n";

const char *psmailfooter = ".\r\n";

struct psmail *mailqueue;
char psm_sendbuf[3000];
int psm_sbufsize;
int psm_mailerfd;
int psm_state;

void ps_makereportmail(scanhost *shp) {
  struct psmail *themail;
  int pos=0;
  int i;
  char timebuf[30];
  
  Error("proxyscan",ERR_INFO,"Generating report mail for %s.",IPtostr(shp->IP));
  
  themail=(struct psmail *)malloc(sizeof(struct psmail));
  strcpy(themail->content, psmailtemplate);
  pos=strlen(psmailtemplate);

  for (i=0;i<PSCAN_MAXSCANS;i++) {
    if (shp->scans[i] && shp->scans[i]->outcome == SOUTCOME_OPEN) {
      pos += sprintf(themail->content + pos, "%s: %s:%d\r\n", scantostr(shp->scans[i]->type), IPtostr(shp->IP), shp->scans[i]->port);
    }
  }
  
  strftime(timebuf,30,"%Y-%m-%d %H:%M:%S",gmtime(&(shp->connecttime)));
  pos += sprintf(themail->content + pos, "\r\n%s: %s connected to %s.\r\n", timebuf, shp->hostmask, shp->servername);
  
  strcpy(themail->content + pos, psmailfooter);
  
  if (!mailqueue) {
    /* There's no mail queue outstanding atm, so let's kick the machinery into action */
    if (psm_state == PSM_IDLE && psm_mailerfd>-1) {
      psm_sbufsize = sprintf(psm_sendbuf,"MAIL FROM: <proxyscan@quakenet.org>\r\n");
      psm_state = PSM_SENTFROM;

      if (ps_flushbuf()) {
	Error("proxyscan",ERR_DEBUG,"Error sending new mail to MTA.");
	ps_mailconnect();
      }
    } else if (psm_state == PSM_NOTCONNECTED) {
      /* Initiate connection */
      ps_mailconnect();
    } else {
      Error("proxyscan",ERR_ERROR,"Unexpected happened: Not idle or disconnected but no mail outstanding?");
    }
  }
  
  themail->next=mailqueue;
  mailqueue=themail;
}

void psm_handler(int fd, short revents) {
  char buf[256];
  int res;
  struct psmail *themail;

  /* Error, abandon socket and reconnect if mail pending */
  if (revents & (POLLERR | POLLHUP)) {
    deregisterhandler(psm_mailerfd, 1);
    psm_mailerfd=-1;
    psm_state = PSM_NOTCONNECTED;
    if (mailqueue) 
      ps_mailconnect();
    return;
  }
  
  if (revents & POLLIN) {
    /* Got some form of response.. */
    
    res=read(fd, buf, 255);
    if (res==0) {
      /* Read EOF, not good - reconnect if mail is waiting */
      deregisterhandler(psm_mailerfd, 1);
      psm_mailerfd=-1;
      psm_state = PSM_NOTCONNECTED;
      if (mailqueue) 
	ps_mailconnect();
      return;
    }

    if (res==-1) {
      if (errno==EAGAIN || errno==EINPROGRESS) {
	/* OK, ran out of data */
	return;
      }
      
      /* otherwise, error */
      deregisterhandler(psm_mailerfd, 1);
      psm_state = PSM_NOTCONNECTED;
      psm_mailerfd=-1;
      if (mailqueue)
        ps_mailconnect();
      
      return;
    }

    /* OK, we got some actual response.  Let's assume
     * that it was an "OK" or similar. */
    
    buf[res-1]='\0';
    Error("proxyscan",ERR_DEBUG,"MTA response: %s",buf);

    switch (psm_state) {
    case PSM_CONNECTING:
      /* Got banner, say HELO */
      psm_sbufsize = sprintf(psm_sendbuf,"HELO %s\r\n",ps_mailname->content);
      psm_state = PSM_SENTHELLO;
      break;

    case PSM_SENTHELLO:
      /* Said HELLO, so announce mail */
      if (!mailqueue) {
	/* Should never happen */
	psm_state = PSM_IDLE;
      } else {
	psm_sbufsize = sprintf(psm_sendbuf,"MAIL FROM: <proxyscan@quakenet.org>\r\n");
	psm_state = PSM_SENTFROM;
      }
      break;
      
    case PSM_SENTFROM:
      /* Sent FROM, now TO */
      psm_sbufsize = sprintf(psm_sendbuf,"RCPT TO: <splidge@quakenet.org>\r\n");
      psm_state = PSM_SENTTO1;
      break;

    case PSM_SENTTO1:
      /* Sent first TO, now the second one */
      psm_sbufsize = sprintf(psm_sendbuf,"RCPT TO: <bopm@reports.blitzed.org>\r\n");
      psm_state = PSM_SENTTO2;
      break;

    case PSM_SENTTO2:
      /* Sent TO, now DATA */
      psm_sbufsize = sprintf(psm_sendbuf,"DATA\r\n");
      psm_state = PSM_SENTDATA;
      break;

    case PSM_SENTDATA:
      /* Sent data, now send the body */
      strcpy(psm_sendbuf, mailqueue->content);
      psm_sbufsize=strlen(mailqueue->content);
      psm_state = PSM_SENTBODY;
      break;
      
    case PSM_SENTBODY:
      /* The body went through OK, so let's junk this mail and move onto the next one */
      themail = mailqueue;
      mailqueue=themail->next;
      free(themail);
      
      if (mailqueue) {
	/* Mail waiting */
	psm_sbufsize = sprintf(psm_sendbuf,"MAIL FROM: <proxyscan@quakenet.org>\r\n");
	psm_state = PSM_SENTFROM;
      } else {
	psm_sbufsize=0;
	psm_state = PSM_IDLE;
      }
      
      break;
    }
    
    /* If we generated a response, send it */
    if (psm_sbufsize) {
      if (ps_flushbuf()) {
	/* We shouldn't ever get a "partial flush" so treat it as an error */
	deregisterhandler(psm_mailerfd,1);
	psm_state = PSM_NOTCONNECTED;
      }
    }
  }
}

void ps_mailconnect() {
  Error("proxyscan",ERR_INFO,"Attempting MTA connection to %s:%d",IPtostr(ps_mailip), ps_mailport);
  psm_mailerfd=createconnectsocket(ps_mailip, ps_mailport);
  psm_state = PSM_CONNECTING;
  /* We wait for the connection banner */
  registerhandler(psm_mailerfd,POLLIN,psm_handler);
}

/*
 * ps_flushbuf(): tries to flush the write queue
 *
 * -1 : EOF or socket error
 *  0 : All data flushed
 *  1 : Partial data flushed
 */

int ps_flushbuf() {
  int res;
  
  if (psm_mailerfd==-1)
    return -1;
  
  res=write(psm_mailerfd, psm_sendbuf, psm_sbufsize);
  
  if (res==0) {
    /* EOF */
    close(psm_mailerfd);
    psm_mailerfd=-1;
    return -1;
  }
  
  if (res==-1) {
    if (errno==EAGAIN || errno==EINPROGRESS)
      return 1;
   
    close(psm_mailerfd);
    psm_mailerfd=-1; 
    return -1;
  }
  
  if (res==psm_sbufsize) {
    psm_sbufsize=0;
    return 0;
  }
  
  /* Partial write */
  memmove(psm_sendbuf, psm_sendbuf+res, psm_sbufsize-res);
  psm_sbufsize-=res;
  
  return 1;
}
