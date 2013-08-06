#ifndef __WHOWAS_H
#define __WHOWAS_H

#define WW_MAXAGE 3600
#define WW_MAXENTRIES 100000
#define WW_MASKLEN (HOSTLEN + USERLEN + NICKLEN)
#define WW_REASONLEN 512

typedef struct whowas {
  int type;
  time_t timestamp;
  nick *nick; /* unlinked nick */

  /* WHOWAS_QUIT or WHOWAS_KILL */
  sstring *reason;

  /* WHOWAS_RENAME */
  sstring *newnick;

  unsigned int marker;

  struct whowas *next;
  struct whowas *prev;
} whowas;

extern whowas *whowas_head, *whowas_tail;
extern int whowas_count;

#define WHOWAS_QUIT 0
#define WHOWAS_KILL 1
#define WHOWAS_RENAME 2

whowas *whowas_fromnick(nick *np);
nick *whowas_tonick(whowas *ww);
void whowas_freenick(nick *np);
whowas *whowas_chase(const char *nick, int maxage);
const char *whowas_format(whowas *ww);
void whowas_free(whowas *ww);

unsigned int nextwhowasmarker(void);

#endif /* __WHOWAS_H */
