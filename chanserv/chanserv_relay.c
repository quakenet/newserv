#include "chanserv.h"
#include "../control/control.h"
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "../core/config.h"
#include "../lib/hmac.h"
#include "../lib/strlfunc.h"
#include "../lib/cbc.h"
#include "authlib.h"

#include <stdio.h>
#include <string.h>

#define KEY_BITS 256
#define BLOCK_SIZE 16

MODULE_VERSION(QVERSION);

int csa_docheckhashpass(void *source, int cargc, char **cargv);
int csa_docreateaccount(void *source, int cargc, char **cargv);
int csa_dosettempemail(void *source, int cargc, char **cargv);
int csa_doresendemail(void *source, int cargc, char **cargv);
int csa_doactivateuser(void *source, int cargc, char **cargv);
static int decrypt_password(unsigned char *secret, int keybits, char *buf, int bufsize, char *encrypted);
static int hex_to_int(char *input, unsigned char *buf, int buflen);

static unsigned char createaccountsecret[KEY_BITS / 8];
static int createaccountsecret_ok = 0;

static unsigned char hexlookup[256] = {
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
                       0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
                       0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c,
                       0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff
                                  };

void _init(void) {
  sstring *s;

  registercontrolhelpcmd("checkhashpass", NO_RELAY, 3, csa_docheckhashpass, "Usage: checkhashpass <username> <digest> ?junk?");
  registercontrolhelpcmd("settempemail", NO_RELAY, 2, csa_dosettempemail, "Usage: settempemail <userid> <email address>");
  registercontrolhelpcmd("resendemail", NO_RELAY, 1, csa_doresendemail, "Usage: resendemail <userid>");
  registercontrolhelpcmd("activateuser", NO_RELAY, 1, csa_doactivateuser, "Usage: activateuser <userid>");

  s=getcopyconfigitem("chanserv","createaccountsecret","",128);
  if(!s || !s->content || !s->content[0]) {
    Error("chanserv_relay",ERR_WARNING,"createaccountsecret not set, createaccount disabled.");
  } else if(s->length != KEY_BITS / 8 * 2 || !hex_to_int(s->content, createaccountsecret, sizeof(createaccountsecret))) {
    Error("chanserv_relay",ERR_WARNING,"createaccountsecret must be a %d character hex string.", KEY_BITS / 8 * 2);
  } else {
    registercontrolhelpcmd("createaccount", NO_RELAY, 4, csa_docreateaccount, "Usage: createaccount <execute> <username> <email address> <encrypted password>");
    createaccountsecret_ok = 1;
  }

  freesstring(s);
}

void _fini(void) {
  deregistercontrolcmd("checkhashpass", csa_docheckhashpass);
  deregistercontrolcmd("settempemail", csa_dosettempemail);
  deregistercontrolcmd("resendemail", csa_doresendemail);
  deregistercontrolcmd("activateuser", csa_doactivateuser);

  if(createaccountsecret_ok)
    deregistercontrolcmd("createaccount", csa_docreateaccount);
}

int csa_docheckhashpass(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  reguser *rup;
  char *flags;

  if(cargc<3) {
    controlreply(sender, "CHECKHASHPASS FAIL args");
    return CMD_ERROR;
  }

  if (!(rup=findreguserbynick(cargv[0]))) {
    controlreply(sender, "CHECKHASHPASS FAIL user");
    return CMD_OK;
  }

  flags = printflags(QUFLAG_ALL & rup->flags, ruflags);
  if(UHasSuspension(rup)) {
    controlreply(sender, "CHECKHASHPASS FAIL suspended %s %s %u", rup->username, flags, rup->ID);
  } else if(UIsInactive(rup)) {
    controlreply(sender, "CHECKHASHPASS FAIL inactive %s %s %u", rup->username, flags, rup->ID);
  } else if(!checkhashpass(rup, cargc<3?NULL:cargv[2], cargv[1])) {
    controlreply(sender, "CHECKHASHPASS FAIL digest %s %s %u", rup->username, flags, rup->ID);
  } else {
    controlreply(sender, "CHECKHASHPASS OK %s %s %u %s", rup->username, flags, rup->ID, rup->email?rup->email->content:"-");
  }

  return CMD_OK;
}

static char *email_to_error(char *email) {
  maildomain *mdp, *smdp;
  char *local;
  char *dupemail;
  int found = 0;
  maillock *mlp;
  reguser *ruh;

  switch(csa_checkeboy_r(email)) {
    case -1:               break;
    case QM_EMAILTOOSHORT: return "emailshort";
    case QM_EMAILNOAT:     return "emailinvalid";
    case QM_EMAILATEND:    return "emailinvalid";
    case QM_EMAILINVCHR:   return "emailinvalid";
    case QM_NOTYOUREMAIL:  return "emailnotyours";
    case QM_INVALIDEMAIL:  return "emailinvalid";
    default:               return "emailunknown";
  }

  /* maildomain BS... c&p from hello.c */
  for(mlp=maillocks;mlp;mlp=mlp->next) {
    if(!match(mlp->pattern->content, email)) {
      return "emaillocked";
    }
  }

  dupemail = strdup(email);
  local=strchr(dupemail, '@');
  if(!local) {
    free(dupemail);
    return "emailunknown";
  }
  *(local++)='\0';

  mdp=findnearestmaildomain(local);
  if(mdp) {
    for(smdp=mdp; smdp; smdp=smdp->parent) {
      if(MDIsBanned(smdp)) {
        free(dupemail);
        return "emaillocked";
      }
      if((smdp->count >= smdp->limit) && (smdp->limit > 0)) {
        free(dupemail);
        return "emaildomainlimit";
      }
    }
  }

  mdp=findmaildomainbydomain(local);
  if(mdp) {
    for (ruh=mdp->users; ruh; ruh=ruh->nextbydomain) {
      if (ruh->localpart)
        if (!strcasecmp(dupemail, ruh->localpart->content)) {
          found++;
        }
    }

    if((found >= mdp->actlimit) && (mdp->actlimit > 0)) {
      free(dupemail);
      return "emailaddresslimit";
    }
  }

  free(dupemail);

  return NULL;
}

static void sendemail(reguser *rup) {
  csdb_createmail(rup, QMAIL_ACTIVATEEMAIL);
}

int csa_docreateaccount(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  int execute;
  char *error_username = NULL, *error_password = NULL, *error_email = NULL;
  char *username = NULL, *password = NULL, *email = NULL;
  char account_info[512];
  char passbuf[512];
  int do_create;

  if(cargc<4) {
    controlreply(sender, "CREATEACCOUNT FALSE args");
    return CMD_ERROR;
  }

  execute = cargv[0][0] == '1';
  if(strcmp(cargv[1], "0"))
    username = cargv[1];
  if(strcmp(cargv[2], "0"))
    email = cargv[2];
  if(strcmp(cargv[3], "0")) {
    int errorcode = decrypt_password(createaccountsecret, KEY_BITS, passbuf, sizeof(passbuf), cargv[3]);
    if(errorcode) {
      Error("chanserv_relay",ERR_WARNING,"createaccount unable to decrypt password, error code: %d", errorcode);
      controlreply(sender, "CREATEACCOUNT FALSE args");
      return CMD_ERROR;
    }
    password = passbuf;
  }

  if(username) {
    if (findreguserbynick(username)) {
      error_username = "usernameinuse";
    } else if(csa_checkaccountname_r(username)) {
      error_username = "usernameinvalid";
    }
  }

  if(email)
    error_email = email_to_error(email);

  if(password) {
    int r = csa_checkpasswordquality(password);
    if(r == QM_PWTOSHORT) {
      error_password = "passwordshort";
    } else if(r == QM_PWTOLONG) {
      error_password = "passwordlong";
    } else if(r == QM_PWTOWEAK) {
      error_password = "passwordweak";
    } else if(r != -1) {
      error_password = "passwordunknown";
    }
  }

  if(execute && email && password && username && !error_email && !error_password && !error_username) {
    reguser *rup;
    do_create = 1;

    rup = csa_createaccount(username, password, email);
    USetInactive(rup);

    cs_log(sender,"CREATEACCOUNT created auth %s (%s)",rup->username,rup->email->content);
    csdb_createuser(rup);
    snprintf(account_info, sizeof(account_info), " %u", rup->ID);

    sendemail(rup);
  } else {
    account_info[0] = '\0';
    do_create = 0;
  }

  controlreply(sender, "CREATEACCOUNT %s%s%s%s%s%s%s%s",
    do_create ? "TRUE" : "FALSE",
    account_info,
    email && error_email ? " " : "", email && error_email ? error_email : "",
    password && error_password ? " " : "", password && error_password ? error_password : "",
    username && error_username ? " " : "", username && error_username ? error_username : ""
  );

  return CMD_OK;
}

int csa_dosettempemail(void *source, int cargc, char **cargv) {
  char *email;
  char *error;
  reguser *rup;
  nick *sender=(nick *)source;

  if(cargc<2) {
    controlreply(sender, "SETTEMPEMAIL FALSE args");
    return CMD_ERROR;
  }

  rup = findreguserbyID(atoi(cargv[0]));
  if(rup == NULL) {
    controlreply(sender, "SETTEMPEMAIL FALSE useridnotexist");
    return CMD_ERROR;
  }

  if(!UIsInactive(rup)) {
    controlreply(sender, "SETTEMPEMAIL FALSE accountactive");
    return CMD_ERROR;
  }

  email = cargv[1];
  error = email_to_error(email);
  if(error) {
    controlreply(sender, "SETTEMPEMAIL FALSE %s", error);
    return CMD_ERROR;
  }

  freesstring(rup->email);
  rup->email=getsstring(email,EMAILLEN);
  cs_log(sender,"SETTEMPEMAIL OK username %s email %s",rup->username, rup->email->content);

  csdb_updateuser(rup);
  sendemail(rup);

  controlreply(sender, "SETTEMPEMAIL TRUE");

  return CMD_OK;
}

int csa_doresendemail(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=(nick *)source;

  if(cargc<1) {
    controlreply(sender, "RESENDEMAIL FALSE args");
    return CMD_ERROR;
  }

  rup = findreguserbyID(atoi(cargv[0]));
  if(rup == NULL) {
    controlreply(sender, "RESENDEMAIL FALSE useridnotexist");
    return CMD_ERROR;
  }

  if(!UIsInactive(rup)) {
    controlreply(sender, "RESENDEMAIL FALSE accountactive");
    return CMD_ERROR;
  }

  sendemail(rup);
  controlreply(sender, "RESENDEMAIL TRUE");
  cs_log(sender,"RESENDEMAIL OK username %s",rup->username);

  return CMD_OK;
}

int csa_doactivateuser(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=(nick *)source;

  if(cargc<1) {
    controlreply(sender, "ACTIVATEUSER FALSE args");
    return CMD_ERROR;
  }

  rup = findreguserbyID(atoi(cargv[0]));
  if(rup == NULL) {
    controlreply(sender, "ACTIVATEUSER FALSE useridnotexist");
    return CMD_ERROR;
  }

  if(!UIsInactive(rup)) {
    controlreply(sender, "ACTIVATEUSER FALSE accountactive");
    return CMD_ERROR;
  }

  UClearInactive(rup);
  csdb_updateuser(rup);

  cs_log(sender,"ACTIVATEUSER OK username %s",rup->username);
  controlreply(sender, "ACTIVATEUSER TRUE");

  return CMD_OK;
}

static int hex_to_int(char *input, unsigned char *buf, int buflen) {
  int i;
  for(i=0;i<buflen;i++) {
    if((0xff == hexlookup[(int)input[i * 2]]) || (0xff == hexlookup[(int)input[i * 2 + 1]])) {
      return 0;
    } else {
      buf[i] = (hexlookup[(int)input[i * 2]] << 4) | hexlookup[(int)input[i * 2 + 1]];
    }
  }
  return 1;
}

static int decrypt_password(unsigned char *secret, int keybits, char *buf, int bufsize, char *encrypted) {
  size_t ciphertextlen, datalen;
  unsigned char iv[BLOCK_SIZE];
  rijndaelcbc *c;
  int i, j;

  datalen = strlen(encrypted);
  if(datalen % 2 != 0)
    return 1;
  if(datalen < BLOCK_SIZE * 2 * 2)
    return 2;

  if(!hex_to_int(encrypted, iv, BLOCK_SIZE))
    return 3;

  ciphertextlen = (datalen - (BLOCK_SIZE * 2)) / 2;
  if(ciphertextlen > bufsize || ciphertextlen % BLOCK_SIZE != 0)
    return 4;

  if(!hex_to_int(encrypted + BLOCK_SIZE * 2, (unsigned char *)buf, ciphertextlen))
    return 5;

  c = rijndaelcbc_init(secret, keybits, iv, 1);

  for(i=0;i<ciphertextlen;i+=BLOCK_SIZE) {
    char *p = &buf[i];
    unsigned char *r = rijndaelcbc_decrypt(c, (unsigned char *)p);
    memcpy(p, r, BLOCK_SIZE);

    for(j=0;j<BLOCK_SIZE;j++) {
      if(*(p + j) == '\0') {
        rijndaelcbc_free(c);
        return 0;
      }
    }
  }

  rijndaelcbc_free(c);
  return 6;
}
