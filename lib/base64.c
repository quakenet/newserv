/*
 * base64.c: base64 functions
 */
 
#include "base64.h"
#include <assert.h>

int numerictab[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 62, 0, 63, 0, 0, 0, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
  49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

char tokens[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]";

/*
VALID_INLINE long numerictolong(const char *numeric, int numericlen)
{
  long mynumeric=0; 
  int i;

  for (i=0;i<numericlen;i++) {
    mynumeric=(mynumeric << 6)+numerictab[(int) *(numeric++)];
  }

  return mynumeric;
}
*/

char *longtonumeric(long param, int len)
{
  static char mynum[7]; /* Static buffers rock.  Multi-thread at your peril */
  int i;

  /* To go with our marvellous static buffer we 
   * have this rather groovy length limit. */
  assert(len<=6);

  for (i=len-1;i>=0;i--) {
    mynum[i] = tokens[(param % 64)];
    param /= 64;
  }
  mynum[len] = '\0';

  return (mynum);
}

/* Slightly more sane version of the above */

char *longtonumeric2(long param, int len, char *mynum)
{
  int i;

  for (i=len-1;i>=0;i--) {
    mynum[i] = tokens[(param % 64)];
    param /= 64;
  }
  mynum[len] = '\0';

  return (mynum);
}

