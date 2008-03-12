/* flags.h */

#ifndef __FLAGS_H
#define __FLAGS_H

typedef unsigned short flag_t;
#define FLAG_T_SPECIFIER "%hu"

typedef struct {
  unsigned char flagchar;
  flag_t        flagbit;
} flag;

#define REJECT_NONE        0x0000
#define REJECT_UNKNOWN     0x0001
#define REJECT_DISALLOWED  0x0002

int setflags(flag_t *inflags, flag_t flagmask, char *flagstr, const flag *flagslist, short reject);
char *printflags(flag_t inflags, const flag *flaglist);
char *printflagsornone(flag_t inflags, const flag *flaglist);
char *printflags_noprefix(flag_t inflags, const flag *flaglist);
char *printflagdiff(flag_t oldflags, flag_t newflags, const flag *flaglist);

#endif
