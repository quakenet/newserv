#include "hmac.h"
#include <string.h>

void hmacsha256_init(hmacsha256 *c, unsigned char *key, int keylen) {
  unsigned char realkey[64], outerkey[64], innerkey[64];
  SHA256_CTX keyc;
  int i;

  memset(realkey, 0, sizeof(realkey));
  if(keylen > 64) {
    SHA256_Init(&keyc);
    SHA256_Update(&keyc, key, keylen);
    SHA256_Final(realkey, &keyc);
    keylen = 32;
  } else {
    memcpy(realkey, key, keylen);
  }

  /* abusing the cache here, if we do sha256 in between that'll erase it */
  for(i=0;i<64;i++) {
    int r = realkey[i];
    innerkey[i] = r ^ 0x36;
    outerkey[i] = r ^ 0x5c;
  }

  SHA256_Init(&c->outer);
  SHA256_Init(&c->inner);
  SHA256_Update(&c->outer, outerkey, 64);
  SHA256_Update(&c->inner, innerkey, 64);
}

void hmacsha256_update(hmacsha256 *c, unsigned char *message, int messagelen) {
  SHA256_Update(&c->inner, message, messagelen);
}

void hmacsha256_final(hmacsha256 *c, unsigned char *digest) {
  SHA256_Final(digest, &c->inner);
  SHA256_Update(&c->outer, digest, 32);
  SHA256_Final(digest, &c->outer);
}

