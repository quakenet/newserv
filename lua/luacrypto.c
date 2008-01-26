#include "lua.h"
#include "../lib/sha1.h"
#include "../lib/sha2.h"

static int crypto_sha1(lua_State *ps) {
  unsigned char digestbuf[20];
  unsigned char hexbuf[sizeof(digestbuf) * 2 + 1];
  SHA1_CTX c;

  char *s = (char *)lua_tostring(ps, 1);
  int len = lua_strlen(ps, 1);

  SHA1Init(&c);
  SHA1Update(&c, s, len);
  SHA1Final(digestbuf, &c);

  /* hah */
  snprintf(hexbuf, sizeof(hexbuf), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", digestbuf[0], digestbuf[1],  digestbuf[2], digestbuf[3], digestbuf[4], digestbuf[5], digestbuf[6], digestbuf[7], digestbuf[8], digestbuf[9], digestbuf[10], digestbuf[11], digestbuf[12], digestbuf[13], digestbuf[14], digestbuf[15], digestbuf[16], digestbuf[17], digestbuf[18], digestbuf[19]);

  lua_pushstring(ps, hexbuf);
  return 1;
}

static int crypto_sha256(lua_State *ps) {
  unsigned char digestbuf[32];
  unsigned char hexbuf[sizeof(digestbuf) * 2 + 1];
  SHA256_CTX c;

  char *s = (char *)lua_tostring(ps, 1);
  int len = lua_strlen(ps, 1);

  SHA256_Init(&c);
  SHA256_Update(&c, s, len);
  SHA256_Final(digestbuf, &c);

  /* hahahaha */
  snprintf(hexbuf, sizeof(hexbuf), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", digestbuf[0], digestbuf[1],  digestbuf[2], digestbuf[3], digestbuf[4], digestbuf[5], digestbuf[6], digestbuf[7], digestbuf[8], digestbuf[9], digestbuf[10], digestbuf[11], digestbuf[12], digestbuf[13], digestbuf[14], digestbuf[15], digestbuf[16], digestbuf[17], digestbuf[18], digestbuf[19], digestbuf[20], digestbuf[21], digestbuf[22], digestbuf[23], digestbuf[24], digestbuf[25], digestbuf[26], digestbuf[27], digestbuf[28], digestbuf[29], digestbuf[30], digestbuf[31]);

  lua_pushstring(ps, hexbuf);
  return 1;
}

static int crypto_sha384(lua_State *ps) {
  unsigned char digestbuf[48];
  unsigned char hexbuf[sizeof(digestbuf) * 2 + 1];
  SHA384_CTX c;

  char *s = (char *)lua_tostring(ps, 1);
  int len = lua_strlen(ps, 1);

  SHA384_Init(&c);
  SHA384_Update(&c, s, len);
  SHA384_Final(digestbuf, &c);

  /* hahahahahahahahaa */
  snprintf(hexbuf, sizeof(hexbuf), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", digestbuf[0], digestbuf[1],  digestbuf[2], digestbuf[3], digestbuf[4], digestbuf[5], digestbuf[6], digestbuf[7], digestbuf[8], digestbuf[9], digestbuf[10], digestbuf[11], digestbuf[12], digestbuf[13], digestbuf[14], digestbuf[15], digestbuf[16], digestbuf[17], digestbuf[18], digestbuf[19], digestbuf[20], digestbuf[21], digestbuf[22], digestbuf[23], digestbuf[24], digestbuf[25], digestbuf[26], digestbuf[27], digestbuf[28], digestbuf[29], digestbuf[30], digestbuf[31],  digestbuf[32], digestbuf[33], digestbuf[34], digestbuf[35], digestbuf[36], digestbuf[37], digestbuf[38], digestbuf[39], digestbuf[40], digestbuf[41], digestbuf[42], digestbuf[43], digestbuf[44], digestbuf[45], digestbuf[46], digestbuf[47]);

  lua_pushstring(ps, hexbuf);
  return 1;
}

static int crypto_sha512(lua_State *ps) {
  unsigned char digestbuf[64];
  unsigned char hexbuf[sizeof(digestbuf) * 2 + 1];
  SHA512_CTX c;

  char *s = (char *)lua_tostring(ps, 1);
  int len = lua_strlen(ps, 1);

  SHA512_Init(&c);
  SHA512_Update(&c, s, len);
  SHA512_Final(digestbuf, &c);

  /* MUHAHAHAHAHAHAHAHAHAHAAHAHAHAHAHAHHAHAHAAH */
  snprintf(hexbuf, sizeof(hexbuf), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", digestbuf[0], digestbuf[1],  digestbuf[2], digestbuf[3], digestbuf[4], digestbuf[5], digestbuf[6], digestbuf[7], digestbuf[8], digestbuf[9], digestbuf[10], digestbuf[11], digestbuf[12], digestbuf[13], digestbuf[14], digestbuf[15], digestbuf[16], digestbuf[17], digestbuf[18], digestbuf[19], digestbuf[20], digestbuf[21], digestbuf[22], digestbuf[23], digestbuf[24], digestbuf[25], digestbuf[26], digestbuf[27], digestbuf[28], digestbuf[29], digestbuf[30], digestbuf[31],  digestbuf[32], digestbuf[33], digestbuf[34], digestbuf[35], digestbuf[36], digestbuf[37], digestbuf[38], digestbuf[39], digestbuf[40], digestbuf[41], digestbuf[42], digestbuf[43], digestbuf[44], digestbuf[45], digestbuf[46], digestbuf[47], digestbuf[48], digestbuf[49], digestbuf[50], digestbuf[51], digestbuf[52], digestbuf[53], digestbuf[54], digestbuf[55], digestbuf[56], digestbuf[57], digestbuf[58], digestbuf[59], digestbuf[60], digestbuf[61], digestbuf[62], digestbuf[63]);

  lua_pushstring(ps, hexbuf);
  return 1;
}

void lua_registercryptocommands(lua_State *l) {
  lua_register(l, "crypto_sha1", crypto_sha1);
  lua_register(l, "crypto_sha256", crypto_sha256);
  lua_register(l, "crypto_sha384", crypto_sha384);
  lua_register(l, "crypto_sha512", crypto_sha512);
}
