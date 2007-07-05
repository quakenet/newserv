#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

/* formats.c */
void printnick_auth(nick *, nick *);
void printnick_authchans(nick *, nick *);

void _init() {
  regnickdisp("auth", printnick_auth);
  regnickdisp("authchans", printnick_authchans);
}

void _fini() {
  unregnickdisp("auth", printnick_auth);
  unregnickdisp("authchans", printnick_authchans);
}
