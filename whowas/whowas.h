#define WW_MAXAGE 300
#define WW_MAXENTRIES 100000
#define WW_MASKLEN (HOSTLEN + USERLEN + NICKLEN)
#define WW_REASONLEN 512

typedef struct whowas {
  char nick[NICKLEN+1];
  char ident[USERLEN+1];
  char host[HOSTLEN+1];
  char realname[REALLEN+1];
  char reason[WW_REASONLEN+1];

  time_t seen;

  struct whowas *next;
} whowas;
