#ifndef __NOPERSERV_STRUCTS_H
#define __NOPERSERV_STRUCTS_H

typedef unsigned long no_tableid;

typedef struct no_nicklist {
  nick *nick;
  struct no_nicklist *next;
} no_nicklist;

typedef struct no_autheduser {
  unsigned newuser: 1;
  sstring *authname;
  flag_t authlevel;
  flag_t noticelevel;
  no_tableid id;
  struct no_autheduser *next;
  no_nicklist *nick;
} no_autheduser;

int noperserv_load_db(void);
void noperserv_cleanup_db(void);

extern no_autheduser *authedusers;

void noperserv_delete_autheduser(no_autheduser *au);
no_autheduser *noperserv_new_autheduser(char *authname);
no_autheduser *noperserv_get_autheduser(char *authname);
void noperserv_update_autheduser(no_autheduser *au);
void noperserv_add_to_autheduser(nick *np, no_autheduser *au);

unsigned long noperserv_get_autheduser_count(void);
unsigned long noperserv_next_autheduser_id(void);

#endif
