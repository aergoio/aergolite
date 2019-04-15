/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*********************************************************************/

#ifndef SHA256_H
#define SHA256_H

/*************************** HEADER FILES ***************************/
#include <stddef.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/
typedef unsigned char uchar;            // 8-bit byte
typedef unsigned int  uint;             // 32-bit word, change to "long" for 16-bit machines

typedef struct {
	uchar data[64];
	uint datalen;
	unsigned long long bitlen;
	uint state[8];
} SHA256_CTX;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uchar data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uchar hash[]);

#endif   // SHA256_H
