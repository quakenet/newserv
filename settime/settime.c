#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../irc/irc.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "../lib/version.h"

MODULE_VERSION("")

schedule *settime_schedule;
int settime_cmd(void *sender, int cargc, char **cargv);

void settime_sendsettime()
{
time_t newtime;

    newtime = time(NULL);
    irc_send("%s SE %ld", mynumeric->content, newtime);
    setnettime(newtime);
}

void _init()
{
    settime_schedule = schedulerecurring(time(NULL) + 300, 0, 1800, &settime_sendsettime, NULL);
    registercontrolhelpcmd("settime",NO_DEVELOPER,0,&settime_cmd,"Usage: settime\nForce send a settime to network.");

}

void _fini()
{
    deleteschedule(settime_schedule, &settime_sendsettime, NULL);
    deregistercontrolcmd("settime",&settime_cmd);
}

int settime_cmd(void *sender, int cargc, char **cargv) {
   controlreply(sender,"Sending Settime...");
   settime_sendsettime(); 
   controlreply(sender,"Done");
   return CMD_OK;
}
