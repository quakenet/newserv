#include "sha2.h"

typedef struct {
  SHA256_CTX outer, inner;
} hmacsha256;

void hmacsha256_final(hmacsha256 *c, unsigned char *digest);
void hmacsha256_update(hmacsha256 *c, unsigned char *message, int messagelen);
void hmacsha256_init(hmacsha256 *c, unsigned char *key, int keylen);

