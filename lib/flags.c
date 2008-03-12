/* flags.c */

#include "flags.h"

/* 
 * setflags:
 *  Applies a "+x-y+zab" style string to the inflags value.
 *
 * The "flagmask" input restricts the flags that can be changed.
 * The "reject" input specifies whether to silently drop or actively reject if:
 *  REJECT_UNKNOWN:    An unknown flag was specified
 *  REJECT_DISALLOWED: An attempt was made to manipute a flag disallowed by flagmask 
 */
    
int setflags(flag_t *inflags, flag_t flagmask, char *flagstr, const flag *flagslist, short reject) {
  int dir,i;
  flag_t newflags;
  char *ch;
  
  /* Work with a temporary copy */
  newflags=*inflags;
  
  /* Default is to add flags */
  dir=1;
  
  for(ch=flagstr;*ch;ch++) {
    if (*ch=='+') {
      dir=1;
      continue;
    }
    
    if (*ch=='-') {
      dir=0;
      continue;
    }
    
    for(i=0;flagslist[i].flagchar!='\0';i++) {
      if (*ch==flagslist[i].flagchar) {
        /* Check that the change is allowed */
        if (flagmask & flagslist[i].flagbit) {
          if (dir==0) {
            newflags &= (~flagslist[i].flagbit);
          } else {
            newflags |= flagslist[i].flagbit;
          }
        } else {
          if (reject & REJECT_DISALLOWED) {
            return REJECT_DISALLOWED;
          }
        }
        /* We matched, stop looking */
        break;
      }
    }
    
    if (flagslist[i].flagchar=='\0') {
      /* We didn't match anything */
      if (reject & REJECT_UNKNOWN) {
        return REJECT_UNKNOWN;
      }
    }
  }
  
  /* We reached the end without bailing out; assign the flag changes */
  *inflags=newflags;
  return REJECT_NONE;
}

/*
 * printflags:
 *  Prints out which flags are currently set in a flag block
 */
char *printflags(flag_t inflags, const flag *flaglist) {
  static char buf[18];
  int i;
  char *ch=buf;
  
  *ch++='+';
  
  for (i=0;flaglist[i].flagchar!='\0' && i<16;i++) {
    if (inflags&flaglist[i].flagbit) {
      *ch++=flaglist[i].flagchar;
    }
  }
  
  *ch='\0';
  return buf;
}

/*
 * printflagsornone:
 *  Prints out which flags are currently set in a flag block, or return "none"
 */
char *printflagsornone(flag_t inflags, const flag *flaglist) {
  static char buf[18];
  int i;
  char *ch=buf;
  
  *ch++='+';
  
  for (i=0;flaglist[i].flagchar!='\0' && i<16;i++) {
    if (inflags&flaglist[i].flagbit) {
      *ch++=flaglist[i].flagchar;
    }
  }
  
  *ch='\0';
  if (ch==(buf+1)) /* No flags set */
    return "none";
    
  return buf;
}
/* ugh */

char *printflags_noprefix(flag_t inflags, const flag *flaglist) {
  static char buf[18];
  int i;
  char *ch=buf;
  
  for (i=0;flaglist[i].flagchar!='\0' && i<16;i++) {
    if (inflags&flaglist[i].flagbit) {
      *ch++=flaglist[i].flagchar;
    }
  }
  
  *ch='\0';
  return buf;
}



/*
 * printflagdiff:
 *  Prints the flag changes needed to change "oldflags" into "newflags"
 */

char *printflagdiff(flag_t oldflags, flag_t newflags, const flag *flaglist) {
  static char buf[30];
  int i;
  char *ch=buf;
  int chd=0;
 
  /* Removes first */

  for (i=0;flaglist[i].flagchar!='\0' && i<16;i++) {
    if ((oldflags & flaglist[i].flagbit) && !(newflags & flaglist[i].flagbit)) {
      if (chd==0) {
        chd=1;
        *ch++='-';
      }
      *ch++=flaglist[i].flagchar;
    }
  }

  /* Now adds */
  chd=0;

  for (i=0;flaglist[i].flagchar!='\0' && i<16;i++) {
    if (!(oldflags & flaglist[i].flagbit) && (newflags & flaglist[i].flagbit)) {
      if (chd==0) {
	chd=1;
	*ch++='+';
      }
      *ch++=flaglist[i].flagchar;
    }
  }

  *ch='\0';
  return buf;
}
