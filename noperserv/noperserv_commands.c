#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../localuser/localuserchannel.h"
#include "../geoip/geoip.h"

int controlkill(void *sender, int cargc, char **cargv);
int controlopchan(void *sender, int cargc, char **cargv);
int controlkick(void *sender, int cargc, char **cargv);
int controlspewchan(void *sender, int cargc, char **cargv);
int controlspew(void *sender, int cargc, char **cargv);
int controlresync(void *sender, int cargc, char **cargv);
int controlbroadcast(void *sender, int cargc, char **cargv);
int controlobroadcast(void *sender, int cargc, char **cargv);
int controlmbroadcast(void *sender, int cargc, char **cargv);
int controlsbroadcast(void *sender, int cargc, char **cargv);
int controlcbroadcast(void *sender, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("kill", NO_OPER, 2, &controlkill, "Usage: kill <nick> ?reason?\nKill specified user with given reason (or 'Killed').");
  registercontrolhelpcmd("kick", NO_OPER, 3, &controlkick, "Usage: kick <channel> <user> ?reason?\nKick a user from the given channel, with given reason (or 'Kicked').");

  registercontrolhelpcmd("spewchan", NO_OPER, 1, &controlspewchan, "Usage: spewchan <pattern>\nShows all channels which are matched by the given pattern");

/*  registercontrolhelpcmd("spew", NO_OPER, 1, &controlspew, "Usage: spewchan <pattern>\nShows all userss which are matched by the given pattern"); */

  registercontrolhelpcmd("resync", NO_OPER, 1, &controlresync, "Usage: resync <channel>\nResyncs a desynched channel");

  registercontrolhelpcmd("broadcast", NO_OPER, 1, &controlbroadcast, "Usage: broadcast <text>\nSends a message to all users.");
  registercontrolhelpcmd("obroadcast", NO_OPER, 1, &controlobroadcast, "Usage: obroadcast <text>\nSends a message to all IRC operators.");
  registercontrolhelpcmd("mbroadcast", NO_OPER, 2, &controlmbroadcast, "Usage: mbroadcast <mask> <text>\nSends a message to all users matching the mask");
  registercontrolhelpcmd("sbroadcast", NO_OPER, 2, &controlsbroadcast, "Usage: sbroadcast <mask> <text>\nSends a message to all users on specific server(s).");
  registercontrolhelpcmd("cbroadcast", NO_OPER, 2, &controlcbroadcast, "Usage: cbroadcast <2 letter country> <text>\nSends a message to all users in the specified country (GeoIP must be loaded).");
}

void _fini() {
  deregistercontrolcmd("obroadcast", controlobroadcast);
  deregistercontrolcmd("sbroadcast", controlsbroadcast);
  deregistercontrolcmd("mbroadcast", controlmbroadcast);
  deregistercontrolcmd("broadcast", controlbroadcast);

  deregistercontrolcmd("resync", controlresync);
/*   deregistercontrolcmd("spew", controlspew); */
  deregistercontrolcmd("spewchan", controlspewchan);

  deregistercontrolcmd("kill", controlkill); 
  deregistercontrolcmd("kick", controlkick);
}

int controlkick(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  channel *cp;
  nick *target;

  if (cargc<2)
    return CMD_USAGE;

  if ((cp=findchannel(cargv[0]))==NULL) {
    controlreply(np,"Couldn't find that channel.");
    return CMD_ERROR;
  }

  if ((target=getnickbynick(cargv[1]))==NULL) {
    controlreply(np,"Sorry, couldn't find that user");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KICKKILLS, "%s/%s sent KICK for %s!%s@%s on %s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content,cp->index->name->content, (cargc>2)?cargv[2]:"Kicked");
  localkickuser(NULL, cp, target, (cargc>2)?cargv[2]:"Kicked");
  controlreply(sender, "KICK sent.");

  return CMD_OK;
}

/* NO checking to see if it's us or anyone important */
int controlkill(void *sender, int cargc, char **cargv) {
  nick *target;
  nick *np = (nick *)sender;
 
  if (cargc<1)
    return CMD_USAGE;  

  if ((target=getnickbynick(cargv[0]))==NULL) {
    controlreply(np,"Sorry, couldn't find that user.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KICKKILLS, "%s/%s sent KILL for %s!%s@%s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content, (cargc>1)?cargv[1]:"Killed");
  killuser(NULL, target, (cargc>1)?cargv[1]:"Killed");
  controlreply(np, "KILL sent.");

  return CMD_OK;
}

int controlresync(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;
  channel *cp;
  modechanges changes;
  int a;

  if (cargc < 1)
    return CMD_USAGE;

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_MISC, "%s/%s RESYNC'ed %s", np->nick, np->authname, cp->index->name->content);

  irc_send("%s CM %s o", mynumeric->content, cp->index->name->content);

  localsetmodeinit(&changes, cp, mynick);

  for(a=0;a<cp->users->hashsize;a++) {
    if (cp->users->content[a] != nouser) {
      nick *np2 = getnickbynumeric(cp->users->content[a]);

      /* make newserv believe that this user is not opped */
      if (cp->users->content[a] & CUMODE_OP)
        cp->users->content[a] &= ~CUMODE_OP;
      else if (!IsService(np2)) /* if the user wasn't opped before and is not a service we don't care about him */
        continue;

      /* now reop him */
      localdosetmode_nick(&changes, np2, MC_OP);
    }
  }

  localsetmodeflush(&changes, 1);

  controlreply(np, "Done.");

  return CMD_OK;
}

int controlspew(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  return CMD_OK;
}

int controlspewchan(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;
  nick *np2;
  int i, a, ccount = 0, morecservices, ucount, cscount;
  chanindex *cip;
  channel *cp;
  char cservices[300];
  char strcscount[40];

  if (cargc < 1)
    return CMD_USAGE;

  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if ((cp = cip->channel) != NULL && match2strings(cargv[0], cip->name->content)) {
        /* find channel services */
        cservices[0] = '\0';
        cscount = ucount = morecservices = 0;

        for (a=0;a<cp->users->hashsize;a++) {
          if (cp->users->content[a] != nouser) {
            np2 = getnickbynumeric(cp->users->content[a]);

            if (IsService(np2)) {
              cscount++;

              if (strlen(cservices) < 100) {
                if (cservices[0])
                  strlcat(cservices, ", ", sizeof(cservices));
  
                strlcat(cservices, np2->nick, sizeof(cservices));
              } else {
                morecservices++;
              }
            }
            
            ucount++;
          }
        }

        if (morecservices)
          snprintf(cservices + strlen(cservices), sizeof(cservices), " and %d more", morecservices);

        snprintf(strcscount, sizeof(strcscount), "%d", cscount);

        /* TODO: print this channel ;; @slug -- WTF? */
        controlreply(np, "%-30s %5ld %-8s%s%s%-11s%s%s%s",
              cip->name->content,
              ucount,
              ucount != 1 ? "users" : "user",
              *cservices ? "- found " : "",
              *cservices ? strcscount : "",
              *cservices ? (strchr(cservices, ',') ? " chanservs" : " chanserv") : "",
              *cservices ? "(" : "",
              *cservices ? cservices : "",
              *cservices ? ")" : "");

        ccount++;
        
        if (ccount > 1000)
          break; /* Don't ever list more than 1000 channels */
      }
    }
  }

  if (ccount > 1000)
    controlreply(np, "More than 1000 matching channels were found. Please use a more specific pattern.");
  else
    controlreply(np, "Found %d channels matching specified pattern.", ccount);
  
  return CMD_OK;
}

int controlbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;

  if (cargc<1)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent a broadcast: %s", np->nick, np->authname, cargv[0]);

  irc_send("%s O $* :(Broadcast) %s", longtonumeric(mynick->numeric,5), cargv[0]);

  controlreply(np, "Message broadcasted.");

  return CMD_OK;
}

int controlmbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;

  if (cargc<2)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an mbroadcast to %s: %s", np->nick, np->authname, cargv[0], cargv[1]);

  irc_send("%s O $@%s :(mBroadcast) %s", longtonumeric(mynick->numeric,5), cargv[0], cargv[1]);

  controlreply(np, "Message mbroadcasted.");

  return CMD_OK;
}

int controlsbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc<2)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an sbroadcast to %s: %s", np->nick, np->authname, cargv[0], cargv[1]);

  irc_send("%s O $%s :(sBroadcast) %s", longtonumeric(mynick->numeric,5), cargv[0], cargv[1]);

  controlreply(np, "Message sbroadcasted.");

  return CMD_OK;
}

int controlobroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *nip;
  int i;

  if(cargc<1)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an obroadcast: %s", np->nick, np->authname, cargv[0]);

  for(i=0;i<NICKHASHSIZE;i++)
    for(nip=nicktable[i];nip;nip=nip->next)
      if(nip && IsOper(nip))
        controlnotice(nip, "(oBroadcast) %s", cargv[0]);

  controlreply(np, "Message obroadcasted.");

  return CMD_OK;
}

const char GeoIP_country_code[247][3] = { "--","AP","EU","AD","AE","AF","AG","AI","AL","AM","AN","AO","AQ","AR","AS","AT","AU","AW","AZ","BA","BB","BD","BE","BF","BG","BH","BI","BJ","BM","BN","BO","BR","BS","BT","BV","BW","BY","BZ","CA","CC","CD","CF","CG","CH","CI","CK","CL","CM","CN","CO","CR","CU","CV","CX","CY","CZ","DE","DJ","DK","DM","DO","DZ","EC","EE","EG","EH","ER","ES","ET","FI","FJ","FK","FM","FO","FR","FX","GA","GB","GD","GE","GF","GH","GI","GL","GM","GN","GP","GQ","GR","GS","GT","GU","GW","GY","HK","HM","HN","HR","HT","HU","ID","IE","IL","IN","IO","IQ","IR","IS","IT","JM","JO","JP","KE","KG","KH","KI","KM","KN","KP","KR","KW","KY","KZ","LA","LB","LC","LI","LK","LR","LS","LT","LU","LV","LY","MA","MC","MD","MG","MH","MK","ML","MM","MN","MO","MP","MQ","MR","MS","MT","MU","MV","MW","MX","MY","MZ","NA","NC","NE","NF","NG","NI","NL","NO","NP","NR","NU","NZ","OM","PA","PE","PF","PG","PH","PK","PL","PM","PN","PR","PS","PT","PW","PY","QA","RE","RO","RU","RW","SA","SB","SC","SD","SE","SG","SH","SI","SJ","SK","SL","SM","SN","SO","SR","ST","SV","SY","SZ","TC","TD","TF","TG","TH","TJ","TK","TM","TN","TO","TP","TR","TT","TV","TW","TZ","UA","UG","UM","US","UY","UZ","VA","VC","VE","VG","VI","VN","VU","WF","WS","YE","YT","YU","ZA","ZM","ZR","ZW","A1","A2","O1"};

int controlcbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender, *nip;
  int i, ext, target;

  if(cargc < 2)
    return CMD_USAGE;

  ext = findnickext("geoip");
  if(ext == -1) {
    controlreply(np, "Geoip module not loaded.");
    return CMD_ERROR;
  }

  target = COUNTRY_MIN - 1;
  for(i=COUNTRY_MIN;i<COUNTRY_MAX;i++) {
    if(!strcasecmp(cargv[0], GeoIP_country_code[i])) {
      target = i;
      break;
    }
  }

  if(target == -1) {
    controlreply(np, "Invalid country.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent a cbroadcast %s: %s", np->nick, np->authname, cargv[0], cargv[1]);

  for(i=0;i<NICKHASHSIZE;i++)
    for(nip=nicktable[i];nip;nip=nip->next)
      if(nip && ((int)((long)nip->exts[ext]) == target))
        controlnotice(nip, "(cBroadcast) %s", cargv[1]);

  controlreply(np, "Message cbroadcasted.");

  return CMD_OK;
}
