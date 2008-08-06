/* base64.h:
 *
 * Definitions of the base64 functions
 */

#ifndef __BASE64_H
#define __BASE64_H

extern int numerictab[];

/*
#define numerictolong(x,y)        (donumtolong(x,y,0))
#define donumtolong(x,y,z)        ((y)==0?(z):(donumtolong((x)+1,(y)-1,((z)<<6)+numerictab[(int)*(x)])))
*/

static inline long numerictolong(const char *numeric, int numericlen)
{
  long mynumeric=0;
  int i;

  for (i=0;i<numericlen;i++) {
    mynumeric=(mynumeric << 6)+numerictab[(int) *(numeric++)];
  }

  return mynumeric;
}

char *longtonumeric(long param, int len);
char *longtonumeric2(long param, int len, char *mynum);              
#endif              
