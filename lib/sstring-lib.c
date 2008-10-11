#include <string.h>
#include "sstring.h"

int sstringcompare(sstring *ss1, sstring *ss2) {
  if (ss1->length != ss2->length)
    return -1;

  return strncmp(ss1->content, ss2->content, ss1->length);
}

