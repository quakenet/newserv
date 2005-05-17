/* config.h */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "../lib/array.h"
#include "../lib/sstring.h"

void freeconfig();
void initconfig(char *filename);
void dumpconfig();
void rehashconfig();
array *getconfigitems(char *module, char *key);
sstring *getconfigitem(char *module, char *key);
sstring *getcopyconfigitem(char *module, char *key, char *defaultvalue, int len);

#endif
