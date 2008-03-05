#include "sha2.h"
#include "sha1.h"

typedef struct {
  SHA256_CTX outer, inner;
} hmacsha256;

typedef struct {
  SHA1_CTX outer, inner;
} hmacsha1;

void hmacsha256_final(hmacsha256 *c, unsigned char *digest);
void hmacsha256_update(hmacsha256 *c, unsigned char *message, int messagelen);
void hmacsha256_init(hmacsha256 *c, unsigned char *key, int keylen);

void hmacsha1_final(hmacsha1 *c, unsigned char *digest);
void hmacsha1_update(hmacsha1 *c, unsigned char *message, int messagelen);
void hmacsha1_init(hmacsha1 *c, unsigned char *key, int keylen);

char *hmac_printhex(unsigned char *data, char *out, size_t len);
