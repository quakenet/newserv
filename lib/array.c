/*

@GPL

Operservice 2 - array.c
Arrays are used all over in O2. This .c supplies some functions for handling
them.
(C) Michael Meier 2000-2001 - released under GPL
-----------------------------------------------------------------------------
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include "array.h"
#include "../core/error.h"

#ifndef ARLIM1
#define ARLIM1 100     /* If new memory is needed for an array, this many */
                       /* records will be allocated. */
#endif
#ifndef ARLIM2
#define ARLIM2 150     /* If more than this number of records are free in an */
                       /* array, memory is freed. */
#endif

void array_init(array *a, unsigned int memberlen) {
  a->cursi=0;
  a->maxsi=0;
  a->memberlen=memberlen;
  a->content=NULL;
  a->arlim1=ARLIM1;
  a->arlim2=ARLIM2;
}

void array_setlim1(array *a, unsigned int lim) {
  a->arlim1=lim;
}

void array_setlim2(array *a, unsigned int lim) {
  a->arlim2=lim;
}

int array_getfreeslot(array *a) {
  if ((a->cursi+1)>=a->maxsi) {
    /* Reallocation needed */
    a->maxsi+=a->arlim1;
    a->content=realloc(a->content,a->maxsi*a->memberlen);
    if (a->content==NULL) {
      Error("array",ERR_FATAL,"Couldn't allocate memory for growing array");
      exit(1);
    }
  }
  a->cursi++;
  return (a->cursi-1);
}

void array_delslot(array *a, int slotn) {
  if (slotn >= a->cursi) { return; }
  a->cursi--;
  if (a->cursi!=slotn) {
    memcpy(a->content + (slotn * a->memberlen), a->content + (a->cursi * a->memberlen),a->memberlen);
  }
  if ((a->cursi + a->arlim2) < a->maxsi) {
    /* Let's free some memory */
    a->maxsi = a->cursi + a->arlim1;
    a->content=realloc(a->content,a->maxsi*a->memberlen);
    if (a->content==NULL) {
      Error("array",ERR_FATAL,"Couldn't allocate memory for shrinking array?!?");
      exit(1);
    }
  }
}

void array_free(array *a) {
  free(a->content);
  a->cursi=0;
  a->maxsi=0;
  a->memberlen=0;
  a->content=NULL;
  a->arlim1=ARLIM1;
  a->arlim2=ARLIM2;
}
