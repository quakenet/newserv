/* Cleanroom reference implementation of Helix */

/* HEAVILY modified by Chris Porter to support contexts. */

/* See Ferguson et. al, Fast Software Encryption 2003, or
 * http://www.macfergus.com/helix/helixPreproc.pdf
 */

#include <string.h>
#include "helix.h"

/* some useful macros -- little-endian */
#define B(x,i) ((UCHAR)(((x) >> (8*i)) & 0xFF))
#define BYTE2WORD(b) ( \
	(((WORD)(b)[3] & 0xFF)<<24) | \
	(((WORD)(b)[2] & 0xFF)<<16) | \
	(((WORD)(b)[1] & 0xFF)<<8) | \
	(((WORD)(b)[0] & 0xFF)) \
)
#define WORD2BYTE(w, b) { \
	(b)[3] = B(w,3); \
	(b)[2] = B(w,2); \
	(b)[1] = B(w,1); \
	(b)[0] = B(w,0); \
}
#define XORWORD(w, b) { \
	(b)[3] ^= B(w,3); \
	(b)[2] ^= B(w,2); \
	(b)[1] ^= B(w,1); \
	(b)[0] ^= B(w,0); \
}
#define ROTL(w,x) (((w) << (x))|((w) >> (32 - (x))))

/* 3.2, figure 2, block function */
WORD
h_block(struct helix_ctx *ctx, WORD X_i0, WORD P_i, WORD X_i1)
{
    WORD    r;

    r = ctx->A; /* for returning later */
    ctx->A += ctx->D;	    ctx->D = ROTL(ctx->D, 15);
    ctx->B += ctx->E;	    ctx->E = ROTL(ctx->E, 25);
    ctx->C ^= ctx->A;	    ctx->A = ROTL(ctx->A, 9);
    ctx->D ^= ctx->B;	    ctx->B = ROTL(ctx->B, 10);
    ctx->E += ctx->C;	    ctx->C = ROTL(ctx->C, 17);
    ctx->A ^= ctx->D + X_i0;  ctx->D = ROTL(ctx->D, 30);
    ctx->B ^= ctx->E;	    ctx->E = ROTL(ctx->E, 13);
    ctx->C += ctx->A;	    ctx->A = ROTL(ctx->A, 20);
    ctx->D += ctx->B;	    ctx->B = ROTL(ctx->B, 11);
    ctx->E ^= ctx->C;	    ctx->C = ROTL(ctx->C, 5);
    ctx->A += ctx->D ^ P_i;   ctx->D = ROTL(ctx->D, 15);
    ctx->B += ctx->E;	    ctx->E = ROTL(ctx->E, 25);
    ctx->C ^= ctx->A;	    ctx->A = ROTL(ctx->A, 9);
    ctx->D ^= ctx->B;	    ctx->B = ROTL(ctx->B, 10);
    ctx->E += ctx->C;	    ctx->C = ROTL(ctx->C, 17);
    ctx->A ^= ctx->D + X_i1;  ctx->D = ROTL(ctx->D, 30);
    ctx->B ^= ctx->E;	    ctx->E = ROTL(ctx->E, 13);
    ctx->C += ctx->A;	    ctx->A = ROTL(ctx->A, 20);
    ctx->D += ctx->B;	    ctx->B = ROTL(ctx->B, 11);
    ctx->E ^= ctx->C;	    ctx->C = ROTL(ctx->C, 5);
    /* increment i in funny way */
    if (++ctx->h_iplus8[0] == 0x80000000lu) {
	++ctx->h_iplus8[1];
	ctx->h_iplus8[0] = 0;
    }
    return r;
}

/* 3.7 Key schedule.
 * Could do feistel in place, but this follows description in paper.
 */
void
h_key(struct helix_ctx *ctx, unsigned char *U, int U_len)
{
    WORD    k[40]; /* room for key schedule */
    int	    i;

    if (U_len > 32)
	U_len = 32; /* limit size of key */
    memset((void *)k, 0, sizeof k);
    memcpy((void *)&k[32], U, U_len);
    ctx->h_iplus8[0] = ctx->h_iplus8[1] = 0;

    for (i = 32; i < 40; ++i)
	k[i] = BYTE2WORD(((unsigned char *)&k[i])); /* convert to words */
    for (i = 7; i >= 0; --i) {
	ctx->A = k[4*i+4];
	ctx->B = k[4*i+5];
	ctx->C = k[4*i+6];
	ctx->D = k[4*i+7];
	ctx->E = U_len + 64;
	(void)h_block(ctx, 0, 0, 0);
	k[4*i+0] = ctx->A ^ k[4*i+8];
	k[4*i+1] = ctx->B ^ k[4*i+9];
	k[4*i+2] = ctx->C ^ k[4*i+10];
	k[4*i+3] = ctx->D ^ k[4*i+11];
    }
    /* copy into K */
    for (i = 0; i < 8; ++i)
	ctx->K[i] = k[i];
    /* remember length of key */
    ctx->l_u = U_len;
}

/* 3.3, nonce setup */
void
h_nonce(struct helix_ctx *ctx, UCHAR nonce[16])
{
    ctx->N[0] = BYTE2WORD(&nonce[0]);
    ctx->N[1] = BYTE2WORD(&nonce[4]);
    ctx->N[2] = BYTE2WORD(&nonce[8]);
    ctx->N[3] = BYTE2WORD(&nonce[12]);
    ctx->N[4] = 0 - ctx->N[0];
    ctx->N[5] = 1 - ctx->N[1];
    ctx->N[6] = 2 - ctx->N[2];
    ctx->N[7] = 3 - ctx->N[3];
}

/* 3.3, X_i functions */
WORD
X(struct helix_ctx *ctx, int one)
{
    WORD    x = 0;

    if (one) {
	x = ctx->K[(ctx->h_iplus8[0] + 4) & 0x07] + ctx->N[ctx->h_iplus8[0] & 0x07] + ctx->h_iplus8[0];
	if ((ctx->h_iplus8[0] & 0x03) == 3)
	    x += ctx->h_iplus8[1];
	else if ((ctx->h_iplus8[0] & 0x03) == 1)
	    x += ctx->l_u << 2;
    }
    else
	x = ctx->K[ctx->h_iplus8[0] & 0x07];
    return x;
}

/* 3.4 initialisation */
void
h_init(struct helix_ctx *ctx)
{
    int	    i;

    ctx->h_iplus8[0] = ctx->h_iplus8[1] = 0;
    ctx->A = ctx->K[3] ^ ctx->N[0];
    ctx->B = ctx->K[4] ^ ctx->N[1];
    ctx->C = ctx->K[5] ^ ctx->N[2];
    ctx->D = ctx->K[6] ^ ctx->N[3];
    ctx->E = ctx->K[7];
    for (i = 0; i < 8; ++i)
	(void) h_block(ctx, X(ctx, 0), 0, X(ctx, 1));
}

/* 3.5 encryption, and 3.6 compute MAC */
void
h_encrypt(struct helix_ctx *ctx, UCHAR *buf, int n, UCHAR macbuf[16])
{
    UCHAR   b[4];
    WORD    w;
    int	    i;

    h_init(ctx);
    while (n >= 4) {
	w = h_block(ctx, X(ctx, 0), BYTE2WORD(buf), X(ctx, 1));
	XORWORD(w, buf);
	buf += 4;
	n -= 4;
    }
    if (n != 0) {
	/* handle an odd bit at the end */
	for (i = 0; i < n; ++i)
	    b[i] = buf[i];
	for (/*...*/; i < 4; ++i)
	    b[i] = 0;
	w = BYTE2WORD(b);
	w = h_block(ctx, X(ctx,0), w, X(ctx,1));
	XORWORD(w, b);
	for (i = 0; i < n; ++i)
	    buf[i] = b[i];
    }
    /* now compute MAC. Note that "n" is currently l(P) mod 4. */
    ctx->A ^= 0x912d94f1;
    for (i = 0; i < 8; ++i)
	(void) h_block(ctx, X(ctx,0), n, X(ctx,1));
    for (i = 0; i < 4; ++i) {
	w = h_block(ctx, X(ctx,0), n, X(ctx,1));
	WORD2BYTE(w, &macbuf[i*4]);
    }
}

/* 3.8 decryption, and 3.6 compute MAC */
void
h_decrypt(struct helix_ctx *ctx, UCHAR *buf, int n, UCHAR macbuf[16])
{
    UCHAR   b[4];
    WORD    w;
    int	    i;

    h_init(ctx);
    while (n >= 4) {
	/* rather than muck with h_block, we use knowledge of A */
	w = BYTE2WORD(buf) ^ ctx->A; /* plaintext */
	w = h_block(ctx, X(ctx,0), w, X(ctx,1));
	XORWORD(w, buf);
	buf += 4;
	n -= 4;
    }
    if (n != 0) {
	/* handle an odd bit at the end */
	for (i = 0; i < n; ++i)
	    b[i] = buf[i];
	XORWORD(ctx->A, b);
	for (/*...*/; i < 4; ++i)
	    b[i] = 0;
	w = BYTE2WORD(b);
	(void) h_block(ctx, X(ctx,0), w, X(ctx,1)); /* note decryption already done */
	for (i = 0; i < n; ++i)
	    buf[i] = b[i];
    }
    /* now compute MAC. Note that "n" is currently l(P) mod 4. */
    ctx->A ^= 0x912d94f1;
    for (i = 0; i < 8; ++i)
	(void) h_block(ctx, X(ctx,0), n, X(ctx,1));
    for (i = 0; i < 4; ++i) {
	w = h_block(ctx, X(ctx,0), n, X(ctx,1));
	WORD2BYTE(w, &macbuf[i*4]);
    }
}

#ifdef TEST
/*--------------------------------------------------------------------------*/
/* test harness                                                             */
/*--------------------------------------------------------------------------*/

#include "hexlib.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* self test */
void
test_helix(int quick)
{
    extern int	keylen;
    UCHAR	key[32], nonce[16], buf[32], mac[16];
    struct helix_ctx ctx;

    /* basic test */
    printf("Test Vector set 1:\n");
    hexprint("Initial Key", key, 0);
    memset((void *)nonce, 0, 16);
    hexprint("Nonce", nonce, 16);
    h_key(&ctx, key, 0);
    h_nonce(&ctx, nonce);
    hexwprint("Working Key", ctx.K, 32);
    hexwcheck(ctx.K, "a9 3b 6e 32 bc 23 4f 6c 32 6c 0f 82 74 ff a2 41"
		 "e3 da 57 7d ef 7c 1b 64 af 78 7c 38 dc ef e3 de", 32);
    hexwprint("Working N", ctx.N, 32);
    memset(buf, 0, 10);
    hexprint("Plaintext", buf, 10);
    h_encrypt(&ctx, buf, 10, mac);
    hexprint("Ciphertext", buf, 10);
    hexcheck(buf, "70 44 c9 be 48 ae 89 22 66 e4", 10);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "65 be 7a 60 fd 3b 8a 5e 31 61 80 80 56 32 d8 10", 16);
    h_decrypt(&ctx, buf, 10, mac);
    hexprint("decrypted", buf, 10);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "65 be 7a 60 fd 3b 8a 5e 31 61 80 80 56 32 d8 10", 16);

    /* second vector */
    printf("\nTest Vector set 2:\n");
    hexread(key, "00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00"
                 "04 00 00 00 05 00 00 00 06 00 00 00 07 00 00 00", 32);
    hexprint("Initial Key", key, 32);
    hexread(nonce, "00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00", 16);
    hexprint("Nonce", nonce, 16);
    h_key(&ctx, key, 32);
    h_nonce(&ctx, nonce);
    hexwprint("Working Key", ctx.K, 32);
    hexwcheck(ctx.K, "6e e9 a7 6c bd 0b f6 20 a6 d9 b7 59 49 d3 39 95"
		 "04 f8 4a d6 83 12 f9 06 ed d1 a6 98 9e c8 9d 45", 32);
    hexread(buf, "00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00"
                 "04 00 00 00 05 00 00 00 06 00 00 00 07 00 00 00", 32);
    hexprint("Plaintext", buf, 32);
    h_encrypt(&ctx, buf, 32, mac);
    hexprint("Ciphertext", buf, 32);
    hexcheck(buf, "7a 72 a7 5b 62 50 38 0b 69 75 1c d1 28 30 8d 9a"
		  "0c 74 46 a3 bf 3f 99 e6 65 56 b9 c1 18 ca 7d 87", 32);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "e4 e5 49 01 c5 0b 34 e7 80 c0 9c 39 b1 09 a1 17", 16);
    h_decrypt(&ctx, buf, 32, mac);
    hexprint("decrypted", buf, 32);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "e4 e5 49 01 c5 0b 34 e7 80 c0 9c 39 b1 09 a1 17", 16);

    /* third vector */
    printf("\nTest Vector set 3:\n");
    hexread(key, "48 65 6c 69 78", 5);
    hexprint("Initial Key", key, 5);
    hexread(nonce, "30 31 32 33 34 35 36 37 38 39 61 62 63 64 65 66", 16);
    hexprint("Nonce", nonce, 16);
    h_key(&ctx, key, 5);
    h_nonce(&ctx, nonce);
    hexwprint("Working Key", ctx.K, 32);
    hexwcheck(ctx.K, "6c 1e d7 7a cb a3 a1 d2 8f 1c d6 20 6d f1 15 da"
		 "f4 03 28 4a 73 9b b6 9f 35 7a 85 f5 51 32 11 39", 32);
    hexread(buf, "48 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21", 13);
    hexprint("Plaintext", buf, 13);
    h_encrypt(&ctx, buf, 13, mac);
    hexprint("Ciphertext", buf, 13);
    hexcheck(buf, "6c 4c 27 b9 7a 82 a0 c5 80 2c 23 f2 0d", 13);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "6c 82 d1 aa 3b 90 5f 12 f1 44 3f a7 f6 a1 01 d2", 16);
    h_decrypt(&ctx, buf, 13, mac);
    hexprint("decrypted", buf, 13);
    hexprint("MAC", mac, 16);
    hexcheck(mac, "6c 82 d1 aa 3b 90 5f 12 f1 44 3f a7 f6 a1 01 d2", 16);
}

#define BLOCKSIZE	1600	/* for MAC-style tests */
#define MACSIZE		16
char	*testkey = "test key 128bits";
UCHAR	testIV[16];
UCHAR	testframe[BLOCKSIZE];
UCHAR	testmac[16];

/* Perform various timing tests
 */
UCHAR	bigbuf[1024*1024];
UCHAR	macbuf[16];
int
main(int ac, char **av)
{
    int         n, i;
    int		vflag = 0;
    UCHAR	key[32], IV[32];
    int         keysz, IVsz;
    extern int	keylen;
    extern WORD	K[];

    if (ac == 2 && strcmp(av[1], "-test") == 0) {
        test_helix(0);
        return nerrors;
    }
    if (ac >= 2 && strcmp(av[1], "-verbose") == 0) {
	vflag = 1;
	++av, --ac;
    }
    return 0;
}
#endif /* TEST */
