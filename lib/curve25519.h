/* curve25519.h */

#ifndef __LIB_CURVE25519_H
#define __LIB_CURVE25519_H

void curve25519_clamp_secret(unsigned char *secret);
void curve25519_make_public(unsigned char *pub, const unsigned char *secret);
void curve25519_compute_shared(unsigned char *shared, const unsigned char *secret, const unsigned char *mypub, const unsigned char *theirpub, int am_origin);

#endif

