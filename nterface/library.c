/*
  nterface library functions
  Copyright (C) 2003-2004 Chris Porter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <stdarg.h>

#include "../core/config.h"
#include "../lib/irc_string.h"
#include "../lib/sha2.h"
#include "../lib/rijndael.h"

#include "library.h"

unsigned char hexlookup[256] = {
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
                       0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
                       0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c,
                       0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff
                                  };

FILE *random_fd = NULL;

int getcopyconfigitemint(char *section, char *key, int def, int *value) {
  char buf[50];
  sstring *ini;

  snprintf(buf, sizeof(buf), "%d", def);
  ini = getcopyconfigitem(section, key, buf, 6);

  if(!ini)
    return 0;

  if(!protectedatoi(ini->content, value))
    return 0;
  
  return 1;
}

int getcopyconfigitemintpositive(char *section, char *key, int def) {
  int value;
  if(!getcopyconfigitemint(section, key, def, &value))
    return -1;

  if(value < 0)
    return -1;

  return value;
}

int positive_atoi(char *data) {
  int value;
  if(!protectedatoi(data, &value))
    return -1;

  if(value < 0)
    return -1;

  return value;
}

char *challenge_response(char *challenge, char *password) {
  unsigned char buf[32];
  static char output[sizeof(buf) * 2 + 1];
  SHA256_CTX context;

  SHA256_Init(&context);
  SHA256_Update(&context, (unsigned char *)challenge, strlen(challenge));
  SHA256_Update(&context, (unsigned char *)":", 1);
  SHA256_Update(&context, (unsigned char *)password, strlen(password));
  SHA256_Final(buf, &context);

  SHA256_Init(&context);
  SHA256_Update(&context, (unsigned char *)buf, 32);
  SHA256_Final(buf, &context);

  ThirtyTwoByteHex(output, buf);

  return output;
}

int get_entropy(unsigned char *buf, int bytes) {
  if (!random_fd) {
    random_fd = fopen(RANDOM_LOCATION, "rb");
    if(!random_fd)
      return 0;
  }
  
  fread(buf, 1, bytes, random_fd);
  
  return 1;
}

int generate_nonce(unsigned char *nonce, int nterfacer) {
  unsigned char entropy[20], output[32];
  struct timeval tvv;
  SHA256_CTX c;

  if(!get_entropy(entropy, 20))
    return 0;

  gettimeofday(&tvv, NULL);

  SHA256_Init(&c);
  SHA256_Update(&c, entropy, 20);
  SHA256_Update(&c, (unsigned char *)&tvv, sizeof(struct timeval));
  SHA256_Final(output, &c);
  memcpy(nonce, output, 16);

  if(nterfacer) {
    nonce[7]&=128;
  } else {
    nonce[7]|=127;
  }

  return 1;
}

char *int_to_hex(unsigned char *input, char *buf, int len) {
  int i;
  for(i=0;i<len;i++)
    sprintf(buf + i * 2, "%.2x", input[i]);
  *(buf + len * 2) = '\0';
  return buf;
}

int hex_to_int(char *input, unsigned char *buf, int buflen) {
  int i;
  for(i=0;i<buflen;i++) {
    if((0xff == hexlookup[(int)input[i * 2]]) || (0xff == hexlookup[(int)input[i * 2 + 1]])) {
      return 0;
    } else {
      buf[i] = (hexlookup[(int)input[i * 2]] << 4) | hexlookup[(int)input[i * 2 + 1]];
    }
  }
  return 1;
}

char *request_error(int errn) {
  static char err[100];

  err[0] = '\0';
  switch (errn) {
    case RE_MEM_ERROR:
      snc(err, "Memory error");
      break;
    case RE_SERVICE_NOT_FOUND:
      snc(err, "Service not found on this nterfaced instance");
      break;
    case RE_REQUEST_REJECTED:
      snc(err, "Transport handler rejected request");
      break;
    case RE_CONNECTION_CLOSED:
      snc(err, "Transport handler closed connection");
      break;
    case RE_TRANSPORT_NOT_FOUND:
      snc(err, "Transport module not yet loaded for this service");
      break;
    case RE_COMMAND_NOT_FOUND:
      snc(err, "Command not found on this service");
      break;
    case RE_WRONG_ARG_COUNT:
      snc(err, "Wrong number of arguments for this command");
      break;
    case RE_TOO_MANY_ARGS:
      snc(err, "Too many arguments supplied");
      break;
    case RE_SERVICER_NOT_FOUND:
      snc(err, "Service not found on this nterfacer instance");
      break;
    default:
      snc(err, "Unable to find error message");
  }
  err[sizeof(err) - 1] = '\0';

  return err;
}

rijndaelcbc *rijndaelcbc_init(unsigned char *key, int keybits, unsigned char *iv, int decrypt) {
  rijndaelcbc *ret = (rijndaelcbc *)malloc(sizeof(rijndaelcbc) + RKLENGTH(keybits) * sizeof(unsigned long));
  if(!ret)
    return NULL;

  ret->rk = (unsigned long *)(ret + 1);

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

/* digest must be 32 bytes */
void hmacsha256_final(hmacsha256 *c, unsigned char *digest) {
  SHA256_Final(digest, &c->inner);
  SHA256_Update(&c->outer, digest, 32);
  SHA256_Final(digest, &c->outer);
}

