#include <string.h>
#include <stdio.h>
//#include "chacha.h"

typedef unsigned char u8;
typedef unsigned int u32;

#define U8C(v) (v##U)
#define U32C(v) (v##U)

//#define U8V(v) ((u8)(v) & U8C(0xFF))
//#define U32V(v) ((u32)(v) & U32C(0xFFFFFFFF))

#ifdef SQLITE_BIGENDIAN

#define U8TO32_LITTLE(p) \
          (*((u32*)(p)))

#define U32TO8_LITTLE(p, v) \
        do { \
          *((u32*)(p)) = v; \
        } while (0)

#else

#define U8TO32_LITTLE(p)          \
        ( ((u32)((p)[0])      ) | \
          ((u32)((p)[1]) <<  8) | \
          ((u32)((p)[2]) << 16) | \
          ((u32)((p)[3]) << 24) )

#define U32TO8_LITTLE(p, v)            \
        { (p)[0] = ((v)      ) & 0xff; \
          (p)[1] = ((v) >>  8) & 0xff; \
          (p)[2] = ((v) >> 16) & 0xff; \
          (p)[3] = ((v) >> 24) & 0xff; }

#endif

#if defined(_MSC_VER)
#pragma intrinsic(_rotl)  /* force the compiler to use the processor instruction instead of a function */
#define ROTL32(x, n) _rotl(x, n)
#else
#define ROTL32(v, n) ((v << n) | (v >> (32 - n)))
#endif

#define ROTATE(v, c) ROTL32((v), (c))
#define XOR(v, w) ((v) ^ (w))
#define PLUS(x, y) ((x) + (y))

#define QUARTERROUND(a,b,c,d) \
  x[a] = PLUS(x[a],x[b]); x[d] = ROTATE(XOR(x[d],x[a]),16); \
  x[c] = PLUS(x[c],x[d]); x[b] = ROTATE(XOR(x[b],x[c]),12); \
  x[a] = PLUS(x[a],x[b]); x[d] = ROTATE(XOR(x[d],x[a]), 8); \
  x[c] = PLUS(x[c],x[d]); x[b] = ROTATE(XOR(x[b],x[c]), 7);

static void ChaChaCore(
    unsigned char output[64],
    const u32 input[16],
    int num_rounds
){
    u32 x[16];
    int i;

    memcpy(x, input, sizeof(u32) * 16);

    for (i = num_rounds; i > 0; i -= 2) {
        QUARTERROUND( 0, 4, 8,12)
        QUARTERROUND( 1, 5, 9,13)
        QUARTERROUND( 2, 6,10,14)
        QUARTERROUND( 3, 7,11,15)
        QUARTERROUND( 0, 5,10,15)
        QUARTERROUND( 1, 6,11,12)
        QUARTERROUND( 2, 7, 8,13)
        QUARTERROUND( 3, 4, 9,14)
    }

    for (i = 0; i < 16; ++i) {
        x[i] = PLUS(x[i], input[i]);
    }
    for (i = 0; i < 16; ++i) {
        U32TO8_LITTLE(output + 4 * i, x[i]);
    }
}

static const unsigned char sigma[16] = "A326oL1t3_r0CK5!";   // "expand 32-byte k"

/* the iv can be 0, 4, 8 or 12 bytes */

void chacha_encrypt(
    unsigned char *out,
    const unsigned char *in, unsigned int inLen,
    const unsigned char key[32],
    const unsigned char *iv, const unsigned int ivlen,
    u32 counter,
    int rounds
){
    unsigned char block[64];
    unsigned int input[16];
    unsigned int i;

    input[4] = U8TO32_LITTLE(key + 0);
    input[5] = U8TO32_LITTLE(key + 4);
    input[6] = U8TO32_LITTLE(key + 8);
    input[7] = U8TO32_LITTLE(key + 12);

    input[8] = U8TO32_LITTLE(key + 16);
    input[9] = U8TO32_LITTLE(key + 20);
    input[10] = U8TO32_LITTLE(key + 24);
    input[11] = U8TO32_LITTLE(key + 28);

    input[0] = U8TO32_LITTLE(sigma + 0);
    input[1] = U8TO32_LITTLE(sigma + 4);
    input[2] = U8TO32_LITTLE(sigma + 8);
    input[3] = U8TO32_LITTLE(sigma + 12);

    input[12] = counter;
    input[13] = ivlen > 0 ? U8TO32_LITTLE(iv + 0) : 0;
    input[14] = ivlen > 4 ? U8TO32_LITTLE(iv + 4) : 0;
    input[15] = ivlen > 8 ? U8TO32_LITTLE(iv + 8) : 0;

    while (inLen >= 64) {
        ChaChaCore(block, input, rounds);
        for (i = 0; i < 64; i++) {
            out[i] = in[i] ^ block[i];
        }
        input[12]++;
        inLen -= 64;
        in += 64;
        out += 64;
    }

    if (inLen > 0) {
        ChaChaCore(block, input, rounds);
        for (i = 0; i < inLen; i++) {
            out[i] = in[i] ^ block[i];
        }
    }
}

#define chacha_decrypt  chacha_encrypt
