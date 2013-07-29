#ifndef __NOPERSERV_H
#define __NOPERSERV_H

#include "../control/control.h"
#include "../noperserv/noperserv_db.h"
#include "../lib/flags.h"

int noperserv_ext;

extern const flag no_userflags[];
extern const flag no_noticeflags[];
extern const flag no_commandflags[];

#define NO_NICKS_PER_WHOIS_LINE 3

#define NOGetAuthedUser(user)  (no_autheduser *)(user->auth ? user->auth->exts[noperserv_ext] : NULL)
#define NOGetAuthLevel(user)   user->authlevel
#define NOGetNoticeLevel(user) user->noticelevel
#define NOMax(a, b) (a>b?a:b)
#define NOMin(a, b) (a<b?b:a)

#endif  
