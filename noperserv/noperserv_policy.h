#ifndef __NOPERSERV_POLICY_H
#define __NOPERSERV_POLICY_H

int noperserv_policy_command_permitted(flag_t level, nick *user);
flag_t noperserv_policy_permitted_noticeflags(no_autheduser *target);
void noperserv_policy_update_noticeflags(flag_t fwas, no_autheduser *target);
flag_t noperserv_policy_permitted_modifications(no_autheduser *au, no_autheduser *target);

#define NO_DEFAULT_LEVEL 0
#define NO_FIRST_USER_LEVEL __NO_DEVELOPER | __NO_OPER | __NO_STAFF | NO_DEFAULT_LEVEL
#define NO_DEFAULT_NOTICELEVEL NL_NOTICES
#define NO_FIRST_USER_DEFAULT_NOTICELEVEL NL_ALL | NO_DEFAULT_NOTICELEVEL

#define NOIsStaff(user)        (NOGetAuthLevel(user) & __NO_STAFF)
#define NOIsTrust(user)        (NOGetAuthLevel(user) & __NO_TRUST)
#define NOIsSec(user)          (NOGetAuthLevel(user) & __NO_SEC)
#define NOIsOper(user)         (NOGetAuthLevel(user) & __NO_OPER)
#define NOIsDeveloper(user)    (NOGetAuthLevel(user) & __NO_DEVELOPER)

#define NOIsLeastStaff(user)   (NOGetAuthLevel(user) & (__NO_DEVELOPER | __NO_OPER | __NO_STAFF))
#define NOIsLeastOper(user)    (NOGetAuthLevel(user) & (__NO_OPER | __NO_DEVELOPER))
#define NOIsLeastTrust(user)   (NOGetAuthLevel(user) & (__NO_DEVELOPER | __NO_TRUST))
#define NOIsLeastSec(user)     (NOGetAuthLevel(user) & (__NO_DEVELOPER | __NO_SEC))

#define NL_PEONIC_FLAGS NL_NOTICES
#define NL_ALL          NL_MANAGEMENT | NL_TRUSTS | NL_KICKS | NL_KILLS | NL_GLINES | NL_HITS | NL_CLONING | NL_CLEARCHAN | NL_FAKEUSERS | NL_BROADCASTS | NL_OPERATIONS | NL_OPERING | NL_ALL_COMMANDS
#define NL_OPER_FLAGS   NL_MANAGEMENT | NL_TRUSTS | NL_KICKS | NL_KILLS | NL_GLINES | NL_HITS | NL_CLONING | NL_CLEARCHAN | NL_FAKEUSERS | NL_BROADCASTS | NL_OPERATIONS | NL_OPERING
#define NL_SEC_FLAGS    NL_CLONING
#define NL_TRUST_FLAGS  NL_TRUSTS | NL_CLONING
#define NL_DEV_FLAGS    NL_ALL_COMMANDS

#endif
