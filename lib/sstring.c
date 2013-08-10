/* sstring.h - Declaration of "static strings" functions */

#include "sstring.h"
#include "../core/nsmalloc.h"

#include <assert.h>
#include <string.h>

sstring *getsstring(const char *inputstr, int maxlen) {
  sstring *retval = NULL;
  int length;

  /* getsstring() on a NULL pointer returns a NULL sstring.. */
  if (inputstr == NULL)
    return NULL;

  length = strlen(inputstr) + 1;

  if (length > maxlen)
    length = maxlen + 1;

  assert(length <= SSTRING_MAX + 1);

  retval = nsmalloc(POOL_SSTRING, sizeof(sstring) + length);

  retval->length = length - 1;
  strncpy(retval->content, inputstr, length - 1);
  retval->content[length - 1] = '\0';

  return retval;
}

void freesstring(sstring *inval) {
  nsfree(POOL_SSTRING, inval);
}

int sstringcompare(sstring *ss1, sstring *ss2) {
  if (ss1->length != ss2->length)
    return -1;

  return strncmp(ss1->content, ss2->content, ss1->length);
}
