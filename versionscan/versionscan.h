#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define VS_NICK "V"
#define VS_IDENT "TheVBot"
#define VS_HOST "versionscan.quakenet.org"
#define VS_RNDESC "VersionScan"
#define VS_AUTHNAME "V"

#define VS_VERSION "1.13"

#define OPER_CHAN "#qnet.support"

#define VSPATTERNLEN 200
#define VSDATALEN 200

/* modes of operation */
#define VS_IDLE 0
#define VS_SCAN 1
#define VS_STAT 2

/* action types */
#define VS_WARN 1
#define VS_KILL 2
#define VS_GLUSER 3
#define VS_GLHOST 4

/* user flags */
#define VS_AUTHED 1
#define VS_STAFF 2
#define VS_OPER 4
#define VS_GLINE 8
#define VS_ADMIN 16 

typedef struct vsstatistic vsstatistic;
struct vsstatistic {
  char* reply;
  unsigned long count;
  vsstatistic* next;
};

typedef struct vspattern vspattern;
struct vspattern {
  char pattern[VSPATTERNLEN+1];
  char data[VSDATALEN+1];
  unsigned char action;
  unsigned long hitcount;
  vspattern* next;
};

typedef struct vsauthdata vsauthdata;
struct vsauthdata {
  char account[ACCOUNTLEN+1];
  unsigned char flags;
  vsauthdata* next;
};

unsigned char versionscan_getlevelbyauth(char* auth);
vsauthdata* versionscan_getauthbyauth(char* auth);
int IsVersionscanBird(nick* np);
int IsVersionscanStaff(nick* np);
int IsVersionscanGlineAccess(nick* np);
int IsVersionscanAdmin(nick* np);
const char* versionscan_flagstochar(unsigned char flags);
void versionscan_addpattern(char* pattern, char* data, unsigned char action);
void versionscan_delpattern(char* pattern);
vspattern* versionscan_getpattern(char* pattern);
void versionscan_newnick(int hooknum, void* arg);
void versionscan_handler(nick* me, int type, void** args);
void versionscan_createfakeuser(void* arg);
