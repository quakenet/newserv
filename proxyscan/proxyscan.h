
#ifndef __PROXYSCAN_H
#define __PROXYSCAN_H

#include "../nick/nick.h"
#include "../lib/splitline.h"
#include <time.h>
#include <stdint.h>

/* string seen when:
 * - a HTTP/SOCKS/... proxy connects directly to our listener (because we told it to)
 * - it's a proxy that forwards directly to a QuakeNet ircd (we get this because we sent PRIVMSG)
 */
#define MAGICSTRING      ".quakenet.org 451 *  :Register first.\r\n"
#define MAGICSTRINGLENGTH (sizeof(MAGICSTRING)-1)

/* string seen after a fake GET request */
#define MAGICROUTERSTRING        "\r\nServer: Mikrotik HttpProxy\r\n"
#define MAGICROUTERSTRINGLENGTH  (sizeof(MAGICROUTERSTRING)-1)

/* string seen if the external program decided it is a proxy */
#define MAGICEXTSTRING        "DETECTED\n"
#define MAGICEXTTRINGLENGTH    (sizeof(MAGICEXTSTRING)-1)

#define PSCAN_MAXSCANS      100

#define P_MAX(a,b) (((a)>(b))?(a):(b))
#define PSCAN_READBUFSIZE   (P_MAX(MAGICSTRINGLENGTH, P_MAX(MAGICROUTERSTRINGLENGTH, MAGICEXTTRINGLENGTH)))*2

#define SSTATE_CONNECTING   0
#define SSTATE_SENTREQUEST  1
#define SSTATE_GOTRESPONSE  2

#define STYPE_SOCKS4        0
#define STYPE_SOCKS5        1
#define STYPE_HTTP          2
#define STYPE_WINGATE       3
#define STYPE_CISCO         4
#define STYPE_DIRECT        5
#define STYPE_ROUTER        6

#define STYPE_EXT           7

#define SOUTCOME_INPROGRESS 0
#define SOUTCOME_OPEN       1
#define SOUTCOME_CLOSED     2

#define SCLASS_NORMAL       0
#define SCLASS_CHECK        1
#define SCLASS_PASS2        2
#define SCLASS_PASS3        3
#define SCLASS_PASS4        4

typedef struct scantype {
  int type;
  int port;
  int hits;
} scantype;

typedef struct extrascan {
  unsigned short port;
  unsigned char type;
  struct extrascan *next;
  struct extrascan *nextbynode;
} extrascan;

typedef struct pendingscan {
  patricia_node_t *node; 
  unsigned short port;
  unsigned char type;
  unsigned char class;
  time_t when;
  struct pendingscan *next;
} pendingscan;

typedef struct foundproxy {
  short type;
  unsigned short port;
  struct foundproxy *next;
} foundproxy;

typedef struct cachehost {
  time_t lastscan;
  foundproxy *proxies;
  int glineid;
  time_t lastgline;
  unsigned char marker;
#if defined(PROXYSCAN_MAIL)
  sstring *lasthostmask; /* Not saved to disk */
  time_t lastconnect;    /* Not saved to disk */
#endif
  struct cachehost *next; 
} cachehost;

typedef struct scan {
  int fd;
  patricia_node_t *node; 
  short type;
  unsigned short port;
  unsigned short state;
  unsigned short outcome;
  unsigned short class;
  struct scan *next;
  void *sch;
  char readbuf[PSCAN_READBUFSIZE];
  int bytesread;
  int totalbytesread;
} scan;

#if defined(PROXYSCAN_MAIL)
extern unsigned int ps_mailip;
extern unsigned int ps_mailport;
extern sstring *ps_mailname;
extern int psm_mailerfd;
#endif

extern int activescans;
extern int maxscans;
extern int numscans;
extern scantype thescans[];
extern int brokendb;
extern int ps_cache_ext;
extern int ps_scan_ext;
extern int ps_extscan_ext;
extern int ps_ready;
extern int rescaninterval;

extern sstring *exthost;
extern int extport;

extern unsigned int normalqueuedscans;
extern unsigned int prioqueuedscans;

extern unsigned int ps_start_ts;

extern unsigned long countpendingscan;

extern unsigned long scanspermin;

/* proxyscancache.c */
cachehost *addcleanhost(time_t timestamp);
cachehost *findcachehost(patricia_node_t *node);
void delcachehost(cachehost *);
void dumpcachehosts();
void loadcachehosts();
unsigned int cleancount();
unsigned int dirtycount();
void cachehostinit(time_t ri);
void scanall(int type, int port);

/* proxyscanalloc.c */
scan *getscan();
void freescan(scan *sp);
cachehost *getcachehost();
void freecachehost(cachehost *chp);
foundproxy *getfoundproxy();
void freefoundproxy(foundproxy *fpp);
pendingscan *getpendingscan();
void freependingscan(pendingscan *psp);
extrascan *getextrascan();
void freeextrascan(extrascan *esp);

/* proxyscanlisten.c */
int openlistensocket(int portnum);
void handlelistensocket(int fd, short events);

/* proxyscanconnect.c */
int createconnectsocket(struct irc_in_addr *ip, int socknum);

/* proxyscandb.c */
void loggline(cachehost *chp, patricia_node_t *node);
void proxyscandbclose();
int proxyscandbinit();
int proxyscandolistopen(void *sender, int cargc, char **cargv);
void proxyscanspewip(nick *mynick, nick *usernick, unsigned long a, unsigned long b, unsigned long c, unsigned long d);
void proxyscanshowkill(nick *mynick, nick *usernick, unsigned long a);
const char *scantostr(int type);

#if defined(PROXYSCAN_MAIL)
/* proxyscanmail.c */
void ps_makereportmail(scanhost *shp);
#endif

/* proxyscanqueue.c */
void queuescan(patricia_node_t *node, short scantype, unsigned short port, char class, time_t when);
void startqueuedscans();

/* proxyscan.c */
void startscan(patricia_node_t *node, int type, int port, int class);
void startnickscan(nick *nick);

/* proxyscanext.c */
unsigned int extrascancount();
void loadextrascans();
extrascan *findextrascan(patricia_node_t *node);
void delextrascan(extrascan *esp);
extrascan *addextrascan(unsigned short port, unsigned char type);

#endif
