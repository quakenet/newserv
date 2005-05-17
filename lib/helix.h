#ifndef __helix_H
#define __helix_H

typedef unsigned long WORD;
typedef unsigned char UCHAR;

#define NONCE_LEN 16
#define MAC_LEN   16

/* HELIX variables */
typedef struct helix_ctx {
WORD	h_iplus8[2];		/* block number maintained in two parts */
WORD	K[8];			/* expanded key */
WORD	N[8];			/* expanded nonce */
int	l_u;			/* length of user key */
WORD	A, B, C, D, E;		/* Z_0..Z_4 in the paper */
} helix_ctx;

#undef TEST

void
h_key(struct helix_ctx *ctx, unsigned char *U, int U_len);
void
h_nonce(struct helix_ctx *ctx, UCHAR nonce[16]);
void
h_encrypt(struct helix_ctx *ctx, UCHAR *buf, int n, UCHAR macbuf[16]);
void
h_decrypt(struct helix_ctx *ctx, UCHAR *buf, int n, UCHAR macbuf[16]);

#endif
