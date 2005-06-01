/* Functions for retrieving stats from the network to local users */

#include "localuser.h"
#include "../irc/numeric.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../core/error.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

int handleserverstats(void *source, int cargc, char **cargv);
int handleserverstatsend(void *source, int cargc, char **cargv);

const int numerics[] = { RPL_STATSCLINE, RPL_STATSCOMMANDS, RPL_STATSCONN, RPL_STATSDLINE,
                     RPL_STATSENGINE, RPL_STATSHLINE, RPL_STATSILINE, RPL_STATSKLINE,
                     RPL_STATSLINKINFO, RPL_STATSLLINE, RPL_STATSOLINE, RPL_STATSQLINE,
                     RPL_STATSSLINE, RPL_STATSULINE, RPL_STATSUPTIME, RPL_STATSVERBOSE,
                     RPL_TEXT, 0 };
void _init() {
  const int *i;
  registernumerichandler(219,&handleserverstats,4);

  for(i=&numerics[0];*i;i++)
    registernumerichandler(*i,&handleserverstats,4);
}

void _fini() {
  const int *i;
  deregisternumerichandler(219,&handleserverstats);

  for(i=&numerics[0];*i;i++)
    deregisternumerichandler(*i,&handleserverstats);
}

/* stats look something like:
 * XX 242 XXyyy :data
 */

int handleserverstats(void *source, int cargc, char **cargv) {
  void *nargs[3];
  nick *target;
  static char outbuffer[BUFSIZE * 2 + 5];
  int numeric = (int)source, i;

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
  
  outbuffer[0] = '\0';

  /* bloody inefficient */
  for(i=3;i<cargc;i++) {
    if(i != 3)
      strcat(outbuffer, " ");
    if(i == cargc - 1)
      strcat(outbuffer, ":");

    strcat(outbuffer, cargv[i]);
  }
  outbuffer[sizeof(outbuffer) - 1] = '\0';

  if(numeric != 219) {
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

