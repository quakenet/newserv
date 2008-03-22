#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

/* formats.c */
void printnick_auth(nick *, nick *);
void printnick_authchans(nick *, nick *);
void printchannel_qusers(nick *, chanindex *);

struct searchNode *qusers_parse(int type, int argc, char **argv);

void _init() {
  regnickdisp("auth", printnick_auth);
  regnickdisp("authchans", printnick_authchans);
  regchandisp("qusers", printchannel_qusers);
  registersearchterm("qusers", qusers_parse);
}

void _fini() {
  unregnickdisp("auth", printnick_auth);
  unregnickdisp("authchans", printnick_authchans);
  unregchandisp("qusers", printchannel_qusers);
  deregistersearchterm("qusers", qusers_parse);
}
