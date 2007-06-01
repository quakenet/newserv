/*
  unencumbered array.c
  dynamic UNORDERED array (yes this got me...)

  will grow by allocchunksize (by default 100) when full and
  new space is requested.

  will shrink by freechunksize (by default 150) if that many
  items are not occupied at the end.

  clean written from the function prototypes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/error.h"

#include "array.h"

void array_init(array *a, unsigned int itemsize) {
  a->content = NULL;
  a->cursi = 0; /* count */

  a->itemsize = itemsize;
  a->capacity = 0;

  /* allochunksize MUST be <= freechunksize */
  a->allocchunksize = 100;
  a->freechunksize = 150;
}

/* should be called free_and_reinit really */
void array_free(array *a) {
  free(a->content);

  array_init(a, a->itemsize);
}

int array_getfreeslot(array *a) {
  if(a->cursi + 1 >= a->capacity) {
    a->capacity += a->allocchunksize;

    /* we can be evil and use the same pointer as we're gonna exit if we fail */
    a->content = realloc(a->content, a->capacity * a->itemsize);

    if(!a->content)
       Error("array", ERR_STOP, "Array resize failed.");
  }

  return a->cursi++;
}

void array_delslot(array *a, unsigned int index) {
  /* if we're not deleting the end item then swap the last item into the deleted items space */
  /* unordered so we can do this, and we can also use memcpy */
  if(--a->cursi != index)
    memcpy((char *)a->content + index * a->itemsize, (char *)a->content + a->cursi * a->itemsize, a->itemsize);

  if(a->cursi + a->freechunksize < a->capacity) {

    a->capacity = a->cursi + a->allocchunksize;

    if(a->capacity == 0) {
      free(a->content);

      a->content = NULL;
    } else {
      void *newp = realloc(a->content, a->capacity * a->itemsize);

      if(!newp) {
        Error("array", ERR_WARNING, "Unable to shrink array!");
      } else {
        a->content = newp;
      }
    }
  }
}

