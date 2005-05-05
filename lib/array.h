/* array.h */

#ifndef __ARRAY_H
#define __ARRAY_H

typedef struct {
  unsigned cursi;
  unsigned maxsi;
  unsigned memberlen;
  unsigned arlim1;
  unsigned arlim2;
  void *content;
} array;

void array_init(array *a, unsigned int memberlen);
void array_setlim1(array *a, unsigned int lim);
void array_setlim2(array *a, unsigned int lim);
int array_getfreeslot(array *a);
void array_delslot(array *a, int slotn);
void array_free(array *a);

#endif
