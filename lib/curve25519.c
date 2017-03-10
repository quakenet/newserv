#include "curve25519-donna.h"
#include "sha2.h"

void curve25519_clamp_secret(unsigned char *secret) {
  secret[0] &= 0xf8;
  secret[31] &= 0x7f;
  secret[31] |= 0x40;
}

void curve25519_make_public(unsigned char *pub, const unsigned char *secret) {
  static const unsigned char basepoint[32] = {9};
  curve25519_donna(pub, secret, basepoint);
}

void curve25519_compute_shared(unsigned char *shared, const unsigned char *secret, const unsigned char *mypub, const unsigned char *theirpub, int am_origin) {
  SHA256_CTX ctx;

  SHA256_Init(&ctx);

  curve25519_donna(shared, secret, theirpub);

  /* Hashing:
   * https://download.libsodium.org/doc/advanced/scalar_multiplication.html
   * used to determine the order.
   */
  SHA256_Update(&ctx, shared, 32);
  if (am_origin) {
    SHA256_Update(&ctx, theirpub, 32);
    SHA256_Update(&ctx, mypub, 32);
  } else {
    SHA256_Update(&ctx, mypub, 32);
    SHA256_Update(&ctx, theirpub, 32);
  }

  SHA256_Final(shared, &ctx);
}

