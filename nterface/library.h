/*
  nterface library functions
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterface_library_H
#define __nterface_library_H

#include "../core/error.h"

#include "../lib/sha2.h"

#define RE_OK                  0x00
#define RE_MEM_ERROR           0x01
#define RE_BAD_LINE            0x02
#define RE_REQUEST_REJECTED    0x03
#define RE_SERVICE_NOT_FOUND   0x04
#define RE_CONNECTION_CLOSED   0x05
#define RE_TRANSPORT_NOT_FOUND 0x06
#define RE_COMMAND_NOT_FOUND   0x07
#define RE_WRONG_ARG_COUNT     0x08
#define RE_TOO_MANY_ARGS       0x09
#define RE_SERVICER_NOT_FOUND  0x0A
#define RE_SOCKET_ERROR        0x0B

#define snc(err, f) strncpy(err, f, sizeof(err) - 1)
#define TwentyByteHex(output, buf) snprintf(output, sizeof(output), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", buf[0], buf[1],  buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19]);
#define SixteenByteHex(output, buf) snprintf(output, sizeof(output), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", buf[0], buf[1],  buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
#define ThirtyTwoByteHex(output, buf) snprintf(output, sizeof(output), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", buf[0], buf[1],  buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19], buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26], buf[27], buf[28], buf[29], buf[30], buf[31]);

#define MemError() Error("nterface", ERR_FATAL, "Memory allocation error, file: %s line: %d", __FILE__, __LINE__);

#define RANDOM_LOCATION "/dev/urandom"

#define MemCheck(x) \
  if(!x) { \
    MemError(); \
    return; \
  }

#define MemCheckR(x, y) \
  if(!x) { \
    MemError(); \
    return y; \
  }

int getcopyconfigitemint(char *section, char *key, int def, int *value);
int getcopyconfigitemintpositive(char *section, char *key, int def);
int protected_atoi(char *buf, int *value);
int positive_atoi(char *data);
char *challenge_response(char *challenge, char *password);
char *request_error(int errn);
int get_entropy(unsigned char *data, int bytes);
int generate_nonce(unsigned char *nonce, int nterfacer);
char *int_to_hex(unsigned char *input, char *buf, int len);
int hex_to_int(char *input, unsigned char *buf, int buflen);

typedef struct {
  unsigned char prevblock[16];
  unsigned char scratch[16];
  int nrounds;
  unsigned long *rk;
} rijndaelcbc;

unsigned char *rijndaelcbc_decrypt(rijndaelcbc *c, unsigned char *ctblock);
unsigned char *rijndaelcbc_encrypt(rijndaelcbc *c, unsigned char *ptblock);
void rijndaelcbc_free(rijndaelcbc *c);
rijndaelcbc *rijndaelcbc_init(unsigned char *key, int keybits, unsigned char *iv, int decrypt);

#endif
