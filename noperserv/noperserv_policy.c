#include "../control/control.h"
#include "noperserv.h"
#include "noperserv_db.h"
#include "noperserv_policy.h"

/* command access */
int noperserv_policy_command_permitted(flag_t level, nick *user) {
  no_autheduser *au;
  if(level == __NO_ANYONE)
    return 1;
  if((level & __NO_OPERED) && !IsOper(user))
    return 0;
  if(level & __NO_AUTHED) {
    if(!IsAccount(user))
      return 0;
    if(level & __NO_ACCOUNT) {
      if(!(au = NOGetAuthedUser(user)))
        return 0;
      if((level & __NO_DEVELOPER) && !NOIsDeveloper(au)) {
        return 0;
      } else if((level & __NO_OPER) && !NOIsLeastOper(au)) {
        return 0;
      } else if((level & __NO_STAFF) && !NOIsLeastStaff(au)) {
        return 0;
      }
      if ((level & __NO_SEC) && !NOIsLeastSec(au))
        return 0;
      if ((level & __NO_TRUST) && !NOIsLeastTrust(au))
        return 0;
      if ((level & __NO_RELAY) && !NOIsLeastRelay(au))
        return 0;
    }
  }

  return 1;
}

/* return userflags permitted for each level */
flag_t noperserv_policy_permitted_noticeflags(no_autheduser *target) {
  flag_t result = NL_PEONIC_FLAGS;
  if(NOIsDeveloper(target))
    result |= NL_DEV_FLAGS;
  if(NOIsLeastOper(target))
    result |= NL_OPER_FLAGS;
  if(NOIsLeastSec(target))
    result |= NL_SEC_FLAGS;
  if(NOIsLeastTrust(target))
    result |= NL_TRUST_FLAGS;

  return result;
}

/* @logic */
/* updates target's noticeflags according to their new userflags, unsetting unpermitted and setting new permitted flags */
void noperserv_policy_update_noticeflags(flag_t fwas, no_autheduser *target) {
  flag_t newflags = NOGetNoticeLevel(target);

  newflags &= noperserv_policy_permitted_noticeflags(target); /* unset flags we're not supposed to have */

  /* and add flags we're allowed */
  if(!(fwas & __NO_OPER) && NOIsLeastOper(target))
    newflags |= NL_OPER_FLAGS;
  if(!(fwas & __NO_SEC) && NOIsLeastSec(target))
    newflags |= NL_SEC_FLAGS;
  if(!(fwas & __NO_TRUST) && NOIsLeastTrust(target))
    newflags |= NL_TRUST_FLAGS;
  if(!(fwas & __NO_DEVELOPER) && NOIsDeveloper(target))
    newflags |= NL_DEV_FLAGS;

  target->noticelevel = newflags;
}

/* is au allowed to modified targets flags */
flag_t noperserv_policy_permitted_modifications(no_autheduser *au, no_autheduser *target) {
  flag_t permitted = 0;

  /* @policy */
  if(NOIsLeastOper(au)) {
    permitted |= NO_OPER_FLAGS;
    if(target == au) /* let opers remove their own oper flag */
      permitted |= __NO_OPER;
  }
  if(target == au) {
    if(NOIsLeastSec(au))
      permitted |= __NO_SEC;
    if(NOIsLeastTrust(au))
      permitted |= __NO_TRUST;
  }

  if(NOIsDeveloper(au))
    permitted |= NO_DEV_FLAGS;

  if((target != au) && NOIsLeastOper(target) && !NOIsDeveloper(au))
    return 0;

  return permitted;
}
