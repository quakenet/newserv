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
#include "../lib/sha1.h"
#include "../lib/helix.h"

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

FILE *challenge_random_fd = NULL;

unsigned char entropybuffer[CHALLENGE_ENTROPYLEN * CHALLENGE_ENTROPYBUF];
int entropy_remaining = 0;

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
  unsigned char buf[20];
  static char output[sizeof(buf) * 2 + 1];
  SHA1_CTX context;

  SHA1Init(&context);
  SHA1Update(&context, (unsigned char *)password, strlen(password));
  SHA1Update(&context, (unsigned char *)" ", 1);
  SHA1Update(&context, (unsigned char *)challenge, strlen(challenge));
  SHA1Final(buf, &context);

  TwentyByteHex(output, buf);

  return output;
}

/* my entropy function from Q */
int get_challenge_entropy(unsigned char *data) {
  if (!challenge_random_fd) {
    challenge_random_fd = fopen(CHALLENGE_RANDOM_LOCATION, "rb");
    if(!challenge_random_fd)
      return 0;
  }
  
  if(!entropy_remaining) {
    if(fread(entropybuffer, 1, sizeof(entropybuffer), challenge_random_fd) != sizeof(entropybuffer))
      return 0;
    entropy_remaining = CHALLENGE_ENTROPYBUF;
  }
  
  memcpy(data, entropybuffer + (CHALLENGE_ENTROPYBUF - entropy_remaining) * CHALLENGE_ENTROPYLEN, CHALLENGE_ENTROPYLEN);
  entropy_remaining--;
  
  return 1;
}

int generate_nonce(unsigned char *nonce, int nterfacer) {
  unsigned char entropy[CHALLENGE_ENTROPYLEN];
  struct timeval tvv;
  if(!get_challenge_entropy(entropy))
    return 0;

  gettimeofday(&tvv, NULL);

  memcpy(nonce, &tvv, sizeof(struct timeval));
  memcpy(nonce + sizeof(struct timeval) - 2, entropy, NONCE_LEN - sizeof(struct timeval) + 2);

  if(nterfacer) {
    nonce[7]&=128;
  } else {
    nonce[7]|=127;
  }

  return 1;
}

char *get_random_hex(void) {
  static char output[CHALLENGE_ENTROPYLEN * 2 + 1];
  unsigned char stored[CHALLENGE_ENTROPYLEN];
  if(get_challenge_entropy(stored)) {
    TwentyByteHex(output, stored);
  } else {
    return NULL;
  }
  return output;
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
