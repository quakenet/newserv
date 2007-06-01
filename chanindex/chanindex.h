#ifndef __CHANINDEX_H
#define __CHANINDEX_H

#include "../lib/sstring.h"

#define  CHANNELHASHSIZE      60000
#define  MAXCHANNELEXTS       7

struct channel;

typedef struct chanindex {
  sstring          *name;
  struct channel   *channel;
  struct chanindex *next;
  unsigned int      marker;
  void             *exts[MAXCHANNELEXTS];
} chanindex;

extern chanindex *chantable[CHANNELHASHSIZE];

chanindex *getchanindex();
void freechanindex(chanindex *cip);
void initchannelindex();
chanindex *findchanindex(const char *name);
chanindex *findorcreatechanindex(const char *name);
void releasechanindex(chanindex *cip);
int registerchanext(const char *name);
int findchanext(const char *name);
void releasechanext(int index);
unsigned int nextchanmarker();

#endif
