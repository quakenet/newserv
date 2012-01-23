#define CS_NODB
#include "../chanserv/chanserv.h"
#undef CS_NODB

#include "../lib/version.h"

MODULE_VERSION(QVERSION);

const flag rcflags[] = {
  { 'a', QCFLAG_AUTOOP },
  { 'b', QCFLAG_BITCH },
  { 'c', QCFLAG_AUTOLIMIT },
  { 'e', QCFLAG_ENFORCE },
  { 'f', QCFLAG_FORCETOPIC },
  { 'g', QCFLAG_AUTOVOICE },
  { 'h', QCFLAG_ACHIEVEMENTS },
  { 'i', QCFLAG_INFO },
  { 'j', QCFLAG_JOINED },
  { 'k', QCFLAG_KNOWNONLY },
  { 'p', QCFLAG_PROTECT },
  { 's', QCFLAG_NOINFO },
  { 't', QCFLAG_TOPICSAVE },
  { 'v', QCFLAG_VOICEALL },
  { 'w', QCFLAG_WELCOME },
  { 'z', QCFLAG_SUSPENDED },
  { '\0', 0 } };

const flag rcuflags[] = {
  { 'a', QCUFLAG_AUTOOP },
  { 'b', QCUFLAG_BANNED },
  { 'd', QCUFLAG_DENY },
  { 'g', QCUFLAG_AUTOVOICE },
  { 'i', QCUFLAG_INFO },
  { 'j', QCUFLAG_AUTOINVITE },
  { 'k', QCUFLAG_KNOWN },
  { 'm', QCUFLAG_MASTER },
  { 'n', QCUFLAG_OWNER },
  { 'o', QCUFLAG_OP },
  { 'p', QCUFLAG_PROTECT },
  { 'q', QCUFLAG_QUIET },
  { 's', QCUFLAG_NOINFO },
  { 't', QCUFLAG_TOPIC },
  { 'v', QCUFLAG_VOICE },
  { 'w', QCUFLAG_HIDEWELCOME },

  { '\0', 0 } };

const flag ruflags[] = {
  { 'a',  QUFLAG_ADMIN },
  { 'c',  QUFLAG_ACHIEVEMENTS },
  { 'd',  QUFLAG_DEV },
  { 'D',  QUFLAG_CLEANUPEXEMPT },
  { 'g',  QUFLAG_GLINE },
  { 'G',  QUFLAG_DELAYEDGLINE },
  { 'h',  QUFLAG_HELPER },
  { 'i',  QUFLAG_INFO },
  { 'L',  QUFLAG_NOAUTHLIMIT },
  { 'n',  QUFLAG_NOTICE },
  { 'o',  QUFLAG_OPER },
  { 'p',  QUFLAG_PROTECT },
  { 'q',  QUFLAG_STAFF },
//  { 's',  QUFLAG_NOINFO },
  { 'I',  QUFLAG_INACTIVE },
  { 'T',  QUFLAG_TRUST },
  { 'z',  QUFLAG_SUSPENDED },
  { '\0', 0 } };

const flag mdflags[] = {
  { 'l', MDFLAG_LIMIT },
  { 'b', MDFLAG_BANNED },
  { 'u', MDFLAG_ACTLIMIT },
  { '\0', 0 } };

u_int64_t cs_accountflagmap(reguser *rup) {
  authname a2, *a = &a2;
  a->flags = 0;

  if(UIsOper(rup))
    SetOperFlag(a);

  if(UIsDev(rup))
    SetDeveloper(a);

  if(UIsAdmin(rup))
    SetAdmin(a);

  if(UIsStaff(rup))
    SetStaff(a);

  if(UIsHelper(rup))
    SetSupport(a);

  return a->flags;
}

u_int64_t cs_accountflagmap_str(char *flags) {
  reguser r2, *r = &r2;

  setflags(&r->flags, QUFLAG_ALL, flags, ruflags, REJECT_NONE);

  return cs_accountflagmap(r);
}
