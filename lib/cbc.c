#include <stdlib.h>
#include <string.h>

#include "cbc.h"
#include "rijndael.h"

rijndaelcbc *rijndaelcbc_init(unsigned char *key, int keybits, unsigned char *iv, int decrypt) {
  rijndaelcbc *ret = (rijndaelcbc *)malloc(sizeof(rijndaelcbc) + RKLENGTH(keybits) * sizeof(unsigned long));
  if(!ret)
    return NULL;

  memcpy(ret->prevblock, iv, 16);

  if(decrypt) {
    ret->nrounds = rijndaelSetupDecrypt(ret->rk, key, keybits);
  } else {
    ret->nrounds = rijndaelSetupEncrypt(ret->rk, key, keybits);
  }

  return ret;
}

void rijndaelcbc_free(rijndaelcbc *c) {
  free(c);
}

unsigned char *rijndaelcbc_encrypt(rijndaelcbc *c, unsigned char *ptblock) {
  int i;
  unsigned char *p = c->prevblock, *p2 = c->scratch;
  for(i=0;i<16;i++)
    *p2++ = *p++ ^ *ptblock++;

  rijndaelEncrypt(c->rk, c->nrounds, c->scratch, c->prevblock);
  return c->prevblock;
}

unsigned char *rijndaelcbc_decrypt(rijndaelcbc *c, unsigned char *ctblock) {
  int i;
  unsigned char *p = c->prevblock, *p2 = c->scratch;

  rijndaelDecrypt(c->rk, c->nrounds, ctblock, c->scratch);

  for(i=0;i<16;i++)
    *p2++^=*p++;

  memcpy(c->prevblock, ctblock, 16);
  return c->scratch;
}

