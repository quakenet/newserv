#include <stdio.h>
#include <strings.h>

#include "chanserv.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../lib/prng.h"
#include "../lib/sha1.h"
#include "../lib/sha2.h"
#include "../lib/md5.h"
#include "../lib/hmac.h"

static prngctx rng;
static sstring *secret, *codesecret;

static sstring *combinesecret(char *str) {
  SHA256_CTX ctx;
  unsigned char digest[32];
  char hexbuf[sizeof(digest) * 2 + 1];

  SHA256_Init(&ctx);
  SHA256_Update(&ctx, (unsigned char *)secret->content, secret->length);
  SHA256_Update(&ctx, (unsigned char *)":", 1);
  SHA256_Update(&ctx, (unsigned char *)str, strlen(str));
  SHA256_Final(digest, &ctx);    

  SHA256_Init(&ctx);
  SHA256_Update(&ctx, digest, sizeof(digest));
  SHA256_Final(digest, &ctx);    
  hmac_printhex(digest, hexbuf, sizeof(digest));

  return getsstring(hexbuf, strlen(hexbuf));
}
 
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

  secret=getcopyconfigitem("chanserv","secret","",128);
  if(!secret || !secret->content || !secret->content[0]) {
    unsigned char buf[32];
    char hexbuf[sizeof(buf) * 2 + 1];

    Error("chanserv",ERR_WARNING,"Shared secret not set, generating a random string...");

    cs_getrandbytes(buf, 32);
    hmac_printhex(buf, hexbuf, sizeof(buf));
    secret=getsstring(hexbuf, strlen(hexbuf));
  }
  codesecret=combinesecret("codegenerator");
}


void chanservcryptofree(void) {
  freesstring(secret);
  freesstring(codesecret);
}

ub4 cs_getrandint(void) {
  return prng(&rng);
}

void cs_getrandbytes(unsigned char *buf, size_t bytes) {
  for(;bytes>0;bytes-=4,buf+=4) {
    ub4 b = cs_getrandint();
    memcpy(buf, &b, 4);
  }
}

const char *cs_cralgorithmlist(void) {
  return "HMAC-MD5 HMAC-SHA-1 HMAC-SHA-256 LEGACY-MD5";
}

int crsha1(char *username, const char *password, const char *challenge, const char *response) {
  char buf[1024];

  SHA1_CTX ctx;
  unsigned char digest[20];
  char hexbuf[sizeof(digest) * 2 + 1];
  hmacsha1 hmac;

  /* not sure how this helps but the RFC says to do it... */
  SHA1Init(&ctx);
  SHA1Update(&ctx, (unsigned char *)password, strlen(password));
  SHA1Final(digest, &ctx);

  snprintf(buf, sizeof(buf), "%s:%s", username, hmac_printhex(digest, hexbuf, sizeof(digest)));

  SHA1Init(&ctx);
  SHA1Update(&ctx, (unsigned char *)buf, strlen(buf));
  SHA1Final(digest, &ctx);

  hmacsha1_init(&hmac, (unsigned char *)hmac_printhex(digest, hexbuf, sizeof(digest)), sizeof(digest) * 2);
  hmacsha1_update(&hmac, (unsigned char *)challenge, strlen(challenge));
  hmacsha1_final(&hmac, digest);

  hmac_printhex(digest, hexbuf, sizeof(digest));

  if(!strcasecmp(hmac_printhex(digest, hexbuf, sizeof(digest)), response))
    return 1;

  return 0;
}

int crsha256(char *username, const char *password, const char *challenge, const char *response) {
  char buf[1024];

  SHA256_CTX ctx;
  unsigned char digest[32];
  char hexbuf[sizeof(digest) * 2 + 1];
  hmacsha256 hmac;

  /* not sure how this helps but the RFC says to do it... */
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, (unsigned char *)password, strlen(password));
  SHA256_Final(digest, &ctx);

  snprintf(buf, sizeof(buf), "%s:%s", username, hmac_printhex(digest, hexbuf, sizeof(digest)));

  SHA256_Init(&ctx);
  SHA256_Update(&ctx, (unsigned char *)buf, strlen(buf));
  SHA256_Final(digest, &ctx);

  hmacsha256_init(&hmac, (unsigned char *)hmac_printhex(digest, hexbuf, sizeof(digest)), sizeof(digest) * 2);
  hmacsha256_update(&hmac, (unsigned char *)challenge, strlen(challenge));
  hmacsha256_final(&hmac, digest);

  if(!strcasecmp(hmac_printhex(digest, hexbuf, sizeof(digest)), response))
    return 1;

  return 0;
}

int crmd5(char *username, const char *password, const char *challenge, const char *response) {
  char buf[1024];

  MD5Context ctx;
  unsigned char digest[16];
  char hexbuf[sizeof(digest) * 2 + 1];
  hmacmd5 hmac;

  /* not sure how this helps but the RFC says to do it... */
  MD5Init(&ctx);
  MD5Update(&ctx, (unsigned char *)password, strlen(password));
  MD5Final(digest, &ctx);

  snprintf(buf, sizeof(buf), "%s:%s", username, hmac_printhex(digest, hexbuf, sizeof(digest)));

  MD5Init(&ctx);
  MD5Update(&ctx, (unsigned char *)buf, strlen(buf));
  MD5Final(digest, &ctx);

  hmacmd5_init(&hmac, (unsigned char *)hmac_printhex(digest, hexbuf, sizeof(digest)), sizeof(digest) * 2);
  hmacmd5_update(&hmac, (unsigned char *)challenge, strlen(challenge));
  hmacmd5_final(&hmac, digest);

  if(!strcasecmp(hmac_printhex(digest, hexbuf, sizeof(digest)), response))
    return 1;

  return 0;
}

int crlegacymd5(char *username, const char *password, const char *challenge, const char *response) {
  MD5Context ctx;
  unsigned char digest[16];
  char hexbuf[sizeof(digest) * 2 + 1];

  MD5Init(&ctx);
  MD5Update(&ctx, (unsigned char *)password, strlen(password));
  MD5Update(&ctx, (unsigned char *)" ", 1);
  MD5Update(&ctx, (unsigned char *)challenge, strlen(challenge));
  MD5Final(digest, &ctx);

  if(!strcasecmp(hmac_printhex(digest, hexbuf, sizeof(digest)), response))
    return 1;

  return 0;
}

CRAlgorithm cs_cralgorithm(const char *algorithm) {
  if(!strcasecmp(algorithm, "hmac-sha-1"))
    return crsha1;

  if(!strcasecmp(algorithm, "hmac-sha-256"))
    return crsha256;

  if(!strcasecmp(algorithm, "hmac-md5"))
    return crmd5;

  if(!strcasecmp(algorithm, "legacy-md5"))
    return crlegacymd5;

  return 0;
}

char *cs_calcchallenge(const unsigned char *entropy) {
  unsigned char buf[20];
  static char hexbuf[sizeof(buf) * 2 + 1];
  SHA1_CTX ctx;

  SHA1Init(&ctx);
  /* we can maybe add a salt here in the future */
  SHA1Update(&ctx, entropy, ENTROPYLEN);
  SHA1Final(buf, &ctx);

  hmac_printhex(buf, hexbuf, 16);

  return hexbuf;
}

int cs_checkhashpass(const char *username, const char *password, const char *junk, const char *hash) {
  MD5Context ctx;
  unsigned char digest[16];
  char hexbuf[sizeof(digest) * 2 + 1], buf[512];

  snprintf(buf, sizeof(buf), "%s %s%s%s", username, password, junk?" ":"", junk?junk:"");

  MD5Init(&ctx);
  MD5Update(&ctx, (unsigned char *)buf, strlen(buf));
  MD5Final(digest, &ctx);

  if(strcasecmp(hash, hmac_printhex(digest, hexbuf, sizeof(digest))))
    return 0;

  return 1;
}

char *csc_generateresetcode(time_t lockuntil, char *username) {
  unsigned char digest[32];
  static char hexbuf[sizeof(digest) * 2 + 1];
  hmacsha256 hmac;
  SHA256_CTX ctx;
  char buf[1024];

  snprintf(buf, sizeof(buf), "%s:%lu", username, lockuntil);

  SHA256_Init(&ctx);
  SHA256_Update(&ctx, (unsigned char *)buf, strlen(buf));
  SHA256_Final(digest, &ctx);

  hmac_printhex(digest, hexbuf, sizeof(digest));

  hmacsha256_init(&hmac, (unsigned char *)codesecret->content, codesecret->length);
  hmacsha256_update(&hmac, (unsigned char *)hexbuf, strlen(hexbuf));
  hmacsha256_final(&hmac, digest);

  hmac_printhex(digest, hexbuf, sizeof(digest));

  return hexbuf;
}
