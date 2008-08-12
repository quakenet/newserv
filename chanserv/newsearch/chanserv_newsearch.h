#ifndef __CHANSERV_NEWSEARCH_H
#define __CHANSERV_NEWSEARCH_H

#include "../../newsearch/newsearch.h"

void printnick_auth(searchCtx *, nick *, nick *);
void printnick_authchans(searchCtx *, nick *, nick *);
void printchannel_qusers(searchCtx *, nick *, chanindex *);
void printauth(searchCtx *, nick *, authname *);

struct searchNode *qusers_parse(searchCtx *, int argc, char **argv);
struct searchNode *qlasthost_parse(searchCtx *, int argc, char **argv);
struct searchNode *qemail_parse(searchCtx *, int argc, char **argv);
struct searchNode *qsuspendreason_parse(searchCtx *, int argc, char **argv);
struct searchNode *qusername_parse(searchCtx *, int argc, char **argv);
struct searchNode *qchanflags_parse(searchCtx *, int argc, char **argv);

#endif
