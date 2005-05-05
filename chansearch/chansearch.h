#ifndef __CHANSEARCH_H
#define __CHANSEARCH_H

#include "../parser/parser.h"
#include "../channel/channel.h"
#include "../nick/nick.h"

typedef int (*SearchFunc)(chanindex *cip, void *arg);
typedef void (*DisplayFunc)(nick *sender, chanindex *cip);

typedef struct filter {
  SearchFunc      sf;         /* Actual search execute function to be filled in by setup func */
  void           *arg;        /* Argument for above to be filled in by setup func */
  int             mallocarg;  /* This should be set by the setup func if arg was malloc()'d (it will cause arg to be freed at the end) */
  int             invert;
} filter;        

/* chansearch.c */
void regchansearchfunc(const char *name, int args, CommandHandler handler);
void unregchansearchfunc(const char *name, CommandHandler handler);
void regchansearchdisp(const char *name, DisplayFunc handler);
void unregchansearchdisp(const char *name, DisplayFunc handler);
  
#endif
