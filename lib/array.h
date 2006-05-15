/* array.h */

#ifndef _ARRAY_H

#define _ARRAY_H

typedef struct array {
  void *content; /* must be called this */
  unsigned int cursi; /* must be called this */
  unsigned int capacity;
  unsigned int itemsize;
  unsigned short allocchunksize;
  unsigned short freechunksize;
} array;

void array_init(array *a, unsigned int itemsize);
void array_free(array *a);
int array_getfreeslot(array *a);
void array_delslot(array *a, unsigned int index);

#define array_setlim1(a, size) (a)->allocchunksize = size;
#define array_setlim2(a, size) (a)->freechunksize = size;

#endif /* _ARRAY_H */
