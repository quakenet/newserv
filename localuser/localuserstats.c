/* Functions for retrieving stats from the network to local users */

#include "localuser.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../lib/version.h"
#include "../lib/stringbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

MODULE_VERSION("")

int handleserverstats(void *source, int cargc, char **cargv);
int handleserverstatsend(void *source, int cargc, char **cargv);

#define RPL_STATSLINKINFO    211
#define RPL_STATSCOMMANDS    212
#define RPL_STATSCLINE       213
#define RPL_STATSNLINE       214 /* unused */
#define RPL_STATSILINE       215
#define RPL_STATSKLINE       216
#define RPL_STATSPLINE       217        /* Undernet extension */
#define RPL_STATSYLINE       218
#define RPL_STATSJLINE       222        /* Undernet extension */
#define RPL_STATSALINE       226        /* Hybrid, Undernet */
#define RPL_STATSQLINE       228        /* Undernet extension */
#define RPL_STATSVERBOSE     236        /* Undernet verbose server list */
#define RPL_STATSENGINE      237        /* Undernet engine name */
#define RPL_STATSFLINE       238        /* Undernet feature lines */
#define RPL_STATSLLINE       241        /* Undernet dynamicly loaded modules */
#define RPL_STATSUPTIME      242
#define RPL_STATSOLINE       243
#define RPL_STATSHLINE       244
#define RPL_STATSTLINE       246        /* Undernet extension */
#define RPL_STATSGLINE       247        /* Undernet extension */
#define RPL_STATSULINE       248        /* Undernet extension */
#define RPL_STATSDEBUG       249        /* Extension to RFC1459 */
#define RPL_STATSCONN        250        /* Undernet extension */
#define RPL_STATSDLINE       275        /* Undernet extension */
#define RPL_STATSRLINE       276        /* Undernet extension */
#define RPL_STATSSLINE       398        /* QuakeNet extension -froo */

#define RPL_ENDOFSTATS       219

const int numerics[] = { RPL_STATSLINKINFO, RPL_STATSCOMMANDS, RPL_STATSCLINE, RPL_STATSNLINE, RPL_STATSILINE, RPL_STATSKLINE, 
                         RPL_STATSPLINE, RPL_STATSYLINE, RPL_STATSJLINE, RPL_STATSALINE, RPL_STATSQLINE, RPL_STATSVERBOSE, 
                         RPL_STATSENGINE, RPL_STATSFLINE, RPL_STATSLLINE, RPL_STATSUPTIME, RPL_STATSOLINE, RPL_STATSHLINE,
                         RPL_STATSTLINE, RPL_STATSGLINE, RPL_STATSULINE, RPL_STATSDEBUG, RPL_STATSCONN, RPL_STATSDLINE, RPL_STATSRLINE, 
                         RPL_STATSSLINE, 0 };

void _init() {
  const int *i;
  registernumerichandler(RPL_ENDOFSTATS,&handleserverstats,4);

  for(i=&numerics[0];*i;i++)
    registernumerichandler(*i,&handleserverstats,4);
}

void _fini() {
  const int *i;
  deregisternumerichandler(RPL_ENDOFSTATS,&handleserverstats);

  for(i=&numerics[0];*i;i++)
    deregisternumerichandler(*i,&handleserverstats);
}

/* stats look something like:
 * XX 242 XXyyy :data
 */

int handleserverstats(void *source, int cargc, char **cargv) {
  void *nargs[3];
  nick *target;
  char outbuffer[BUFSIZE * 2 + 5];
  long numeric = (long)source, i;
  StringBuf buf;

  if (cargc<4) {
    return CMD_OK;
  }

  if (!(target=getnickbynumeric(numerictolong(cargv[2],5)))) {
    Error("localuserchannel",ERR_WARNING,"Got stats for unknown local user %s.",cargv[2]);
    return CMD_OK;
  }
  
  if (homeserver(target->numeric) != mylongnum) {
    Error("localuserchannel",ERR_WARNING,"Got stats for non-local user %s.",target->nick);
    return CMD_OK;
  }
  
  sbinit(&buf, outbuffer, sizeof(outbuffer));

  /* bloody inefficient */
  for(i=3;i<cargc;i++) {
    if(i != 3)
      sbaddchar(&buf, ' ');
    if(i == cargc - 1)
      sbaddchar(&buf, ':');

    sbaddstr(&buf, cargv[i]);
  }
  sbterminate(&buf);

  if(numeric != RPL_ENDOFSTATS) {
    nargs[0]=(void *)cargv[0];
    nargs[1]=(void *)numeric;
    nargs[2]=(void *)outbuffer;
  
    if (umhandlers[target->numeric&MAXLOCALUSER]) {
      (umhandlers[target->numeric&MAXLOCALUSER])(target, LU_STATS, nargs);
    }
  } else {
    if (umhandlers[target->numeric&MAXLOCALUSER]) {
      (umhandlers[target->numeric&MAXLOCALUSER])(target, LU_STATS_END, (void *)outbuffer);
    }
  }

  return CMD_OK;
}

