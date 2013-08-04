#define WW_MAXAGE 3600
#define WW_MAXENTRIES 100000
#define WW_MASKLEN (HOSTLEN + USERLEN + NICKLEN)
#define WW_REASONLEN 512

typedef struct whowas {
  time_t seen;
  char nick[NICKLEN + 1];
  char ident[USERLEN + 1];
  char host[HOSTLEN + 1];
  char realname[REALLEN + 1];

  int type;

  /* WHOWAS_QUIT or WHOWAS_KILL */
  sstring *reason;

  /* WHOWAS_RENAME */
  sstring *newnick;

  struct whowas *next;
  struct whowas *prev;
} whowas;

extern whowas *whowas_head, *whowas_tail;
extern int whowas_count;

#define WHOWAS_QUIT 0
#define WHOWAS_KILL 1
#define WHOWAS_RENAME 2

whowas *whowas_chase(const char *nick, int maxage);
