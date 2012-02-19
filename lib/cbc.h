#ifndef __LIB_CBC_H
#define __LIB_CBC_H

typedef struct {
  unsigned char prevblock[16];
  unsigned char scratch[16];
  int nrounds;
  unsigned long rk[0];
} rijndaelcbc;

unsigned char *rijndaelcbc_decrypt(rijndaelcbc *c, unsigned char *ctblock);
unsigned char *rijndaelcbc_encrypt(rijndaelcbc *c, unsigned char *ptblock);
void rijndaelcbc_free(rijndaelcbc *c);
rijndaelcbc *rijndaelcbc_init(unsigned char *key, int keybits, unsigned char *iv, int decrypt);

#endif
