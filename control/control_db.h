#ifndef __NOPERSERV_STRUCTS_H
#define __NOPERSERV_STRUCTS_H

#include "../authext/authext.h"

typedef unsigned long no_tableid;

typedef struct no_autheduser {
  unsigned newuser: 1;
  authname *authname;
  flag_t authlevel;
  flag_t noticelevel;
} no_autheduser;

int noperserv_load_db(void);
void noperserv_cleanup_db(void);

void noperserv_delete_autheduser(no_autheduser *au);
no_autheduser *noperserv_new_autheduser(unsigned long userid, char *ame);
no_autheduser *noperserv_get_autheduser(authname *an);
void noperserv_update_autheduser(no_autheduser *au);

unsigned long noperserv_get_autheduser_count(void);

#endif
