#include <stdio.h>

#include "chanserv.h"
#include "../core/error.h"
#include "../lib/prng.h"

prngctx rng;

void chanservcryptoinit(void) {
  int ret;

  FILE *e = fopen(ENTROPYSOURCE, "rb");
  if(!e) {
    Error("chanserv",ERR_STOP,"Unable to open entropy source."); 
    /* shouldn't be running now... */
  }

  ret = fread(rng.randrsl, 1, sizeof(rng.randrsl), e);
  fclose(e);

  if(ret != sizeof(rng.randrsl)) {
    Error("chanserv",ERR_STOP,"Unable to read entropy.");
    /* shouldn't be running now... */
  }

  prnginit(&rng, 1);
}

ub4 cs_getrandint(void) {
  return prng(&rng);
}

void cs_getrandbytes(unsigned char *buf, size_t bytes) {
  ub4 b;
  
  for(;bytes>0;bytes-=sizeof(b),buf+=sizeof(b)) {
    ub4 b = cs_getrandint();
    memcpy(buf, &b, sizeof(b));
  }
}
