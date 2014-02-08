#ifndef __WHOWAS_H
#define __WHOWAS_H

#define WW_MAXAGE 3600
#define WW_MAXCHANNELS 20
#define WW_MAXENTRIES 1000000
#define WW_MASKLEN (HOSTLEN + USERLEN + NICKLEN)
#define WW_REASONLEN 512

typedef struct whowas {
  int type;
  time_t timestamp;
  nick nick; /* unlinked nick */
  chanindex *channels[WW_MAXCHANNELS];

  /* WHOWAS_QUIT or WHOWAS_KILL */
  sstring *reason;

  /* WHOWAS_RENAME */
  sstring *newnick;

  unsigned int marker;

  struct whowas *next;
  struct whowas *prev;
} whowas;

extern whowas whowasrecs[WW_MAXENTRIES];
extern int whowasoffset; /* points to oldest record */

#define WHOWAS_UNUSED 0
#define WHOWAS_USED 1
#define WHOWAS_QUIT 2
#define WHOWAS_KILL 3
#define WHOWAS_RENAME 4

whowas *whowas_fromnick(nick *np, int standalone);
nick *whowas_tonick(whowas *ww);
void whowas_freenick(nick *np);
whowas *whowas_chase(const char *target, int maxage);
const char *whowas_format(whowas *ww);
const char *whowas_formatchannels(whowas *ww);
void whowas_clean(whowas *ww);
void whowas_free(whowas *ww);

unsigned int nextwhowasmarker(void);

#endif /* __WHOWAS_H */
