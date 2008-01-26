#ifndef CHANPROFILE_H
#define CHANPROFILE_H

#include "../nick/nick.h"

#define CPHASHSIZE	50000

struct chanprofile {
  unsigned int clones;
  unsigned int hashval;
  unsigned int ccount;
  unsigned int clen;
  nick **nicks;
  struct chanprofile *next;
};

struct chanprofile *cptable[CPHASHSIZE];

#endif
