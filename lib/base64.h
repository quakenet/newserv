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

/* Function defined here for speed.. */
/* slug -- these warnings were getting irritating, since we're on C99 we can now use __inline__ */

/*
static long numerictolong(const char *numeric, int numericlen)
{
  long mynumeric=0;
  int i;

  for (i=0;i<numericlen;i++) {
    mynumeric=(mynumeric << 6)+numerictab[(int) *(numeric++)];
  }

  return mynumeric;
}
*/
            
#ifdef __GNUC__
#define INLINE __attribute((always_inline)) inline
#endif

#ifdef _MSC_VER
#define INLINE __forceinline
#endif

#ifndef INLINE
#define INLINE inline
#endif

INLINE long numerictolong(const char *numeric, int numericlen);
char *longtonumeric(long param, int len);
char *longtonumeric2(long param, int len, char *mynum);              
#endif              
