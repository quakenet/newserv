/* modules.h */

#ifndef __MODULES_H
#define __MODULES_H

#include "../lib/sstring.h"

#include <time.h>

#define MODULENAMELEN 40
#define MODULEDESCLEN 200

typedef struct {
  sstring *name;
  void    *handle;
  const char *version;
  const char *buildid;
  time_t loadedsince;
} module;

void initmodules();
int insmod(char *modulename);
int getindex(char *modulename);
int isloaded(char *modulename);
int rmmod(char *modulename, int close);
char *lsmod(int index, const char **ver, const char **buildid, time_t *loadedsince);
void preparereload(char *modulename);
void reloadmarked(void);
void safereload(char *themodule);
void newserv_shutdown();
void *ndlsym(char *module, char *fn);

extern int newserv_shutdown_pending;

#endif
