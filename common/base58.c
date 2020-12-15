//#include <stdbool.h>
//#include <stddef.h>
//#include <stdint.h>
//#include <string.h>
//#include <sys/types.h>

//#include "base58.h"

static const int8_t b58digits_map[] = {
  -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6,  7, 8,-1,-1,-1,-1,-1,-1,
  -1, 9,10,11,12,13,14,15, 16,-1,17,18,19,20,21,-1,
  22,23,24,25,26,27,28,29, 30,31,32,-1,-1,-1,-1,-1,
  -1,33,34,35,36,37,38,39, 40,41,42,43,-1,44,45,46,
  47,48,49,50,51,52,53,54, 55,56,57,-1,-1,-1,-1,-1,
};

bool base58_decode(const char *b58, size_t b58sz, void *bin, size_t *binszp){
  size_t binsz = *binszp;
  const unsigned char *b58u = (unsigned char*) b58;
  unsigned char *binu = (unsigned char*) bin;
  size_t outisz = (binsz + 3) / 4;
#ifdef _MSC_VER
  uint32_t *outi = sqlite3_malloc(outisz * sizeof(uint32_t));
#else
  uint32_t outi[outisz];
#endif
  uint64_t t;
  uint32_t c;
  size_t i, j;
  uint8_t bytesleft = binsz % 4;
  uint32_t zeromask = bytesleft ? (0xffffffff << (bytesleft * 8)) : 0;
  unsigned zerocount = 0;
  bool retval = false;

  if (!b58sz) b58sz = strlen(b58);

  memset(outi, 0, outisz * sizeof(*outi));

  // leading zeros, just count
  for (i = 0; i < b58sz && b58u[i] == '1'; i++) {
    zerocount++;
  }

  for ( ; i < b58sz; i++)  {
    // high-bit set on invalid digit
    if (b58u[i] & 0x80) goto loc_exit;
    // invalid base58 digit
    if (b58digits_map[b58u[i]] == -1) goto loc_exit;
    c = (unsigned) b58digits_map[b58u[i]];
    for (j = outisz; j--; ) {
      t = ((uint64_t)outi[j]) * 58 + c;
      c = (t & 0x3f00000000) >> 32;
      outi[j] = t & 0xffffffff;
    }
    // output number too big (carry to the next int32)
    if (c) goto loc_exit;
    // output number too big (last int32 filled too far)
    if (outi[0] & zeromask) goto loc_exit;
  }

  j = 0;
  switch (bytesleft) {
    case 3:
      *(binu++) = (outi[0] & 0xff0000) >> 16;
    case 2:
      *(binu++) = (outi[0] &   0xff00) >>  8;
    case 1:
      *(binu++) = (outi[0] &     0xff);
      j++;
    default:
      break;
  }

  for (; j < outisz; j++)  {
    *(binu++) = (outi[j] >> 0x18) & 0xff;
    *(binu++) = (outi[j] >> 0x10) & 0xff;
    *(binu++) = (outi[j] >>    8) & 0xff;
    *(binu++) = (outi[j] >>    0) & 0xff;
  }

  // Count canonical base58 byte count
  binu = (unsigned char*) bin;
  for (i = 0; i < binsz; i++) {
    if (binu[i]) break;
    --*binszp;
  }
  *binszp += zerocount;

  retval = true;

loc_exit:
#ifdef _MSC_VER
  sqlite3_free(outi);
#endif
  return retval;
}

static bool double_sha256(void *hash, const void *data, size_t len){
  uint8_t buf[32];
  if (!data || len<=0) return false;
  sha256(buf, data, len);
  sha256(hash, buf, sizeof(buf));
  return true;
}

int base58_check(const char *base58str, size_t b58sz, const void *bin, size_t binsz){
  unsigned char buf[32];
  const uint8_t *binc = (const uint8_t *) bin;
  unsigned i;

  if (binsz < 4)
    return -4;
  if (!double_sha256(buf, bin, binsz - 4))
    return -2;
  if (memcmp(&binc[binsz - 4], buf, 4))
    return -1;

  // Check number of zeros is correct AFTER verifying checksum (to avoid possibility of accessing base58str beyond the end)
  for (i = 0; binc[i] == '\0' && base58str[i] == '1'; i++)
  {}  // Just finding the end of zeros, nothing to do in loop
  if (binc[i] == '\0' || base58str[i] == '1')
    return -3;

  return binc[0];
}

static const char b58digits_ordered[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool base58_encode(const void *data, size_t binsz, char *b58, size_t *b58sz){
  const uint8_t *bin = (const uint8_t *) data;
  int carry;
  size_t i, j, high, zcount = 0;
  size_t size;
  bool retval = false;

  while (zcount < binsz && !bin[zcount])
    zcount++;

  size = (binsz - zcount) * 138 / 100 + 1;
#ifdef _MSC_VER
  uint8_t *buf = sqlite3_malloc(size * sizeof(uint8_t));
#else
  uint8_t buf[size];
#endif
  memset(buf, 0, size);

  for (i = zcount, high = size - 1; i < binsz; i++, high = j) {
    for (carry = bin[i], j = size - 1; (j > high) || carry; j--) {
      carry += 256 * buf[j];
      buf[j] = carry % 58;
      carry /= 58;
    }
  }

  for (j = 0; j < size && !buf[j]; j++);

  if (*b58sz <= zcount + size - j) {
    *b58sz = zcount + size - j + 1;
    goto loc_exit;
  }

  if (zcount)
    memset(b58, '1', zcount);
  for (i = zcount; j < size; i++, j++)
    b58[i] = b58digits_ordered[buf[j]];
  b58[i] = '\0';
  *b58sz = i + 1;

  retval = true;

loc_exit:
#ifdef _MSC_VER
  sqlite3_free(buf);
#endif
  return retval;
}

#ifdef _MSC_VER
bool base58check_encode(const void *data, size_t datasz, uint8_t ver, char *b58c, size_t *b58c_sz){
  uint8_t *buf, *hash;
  bool retval = false;

  buf = sqlite3_malloc(1 + datasz + 0x20);
  if (!buf) return false;

  hash = &buf[1 + datasz];

  buf[0] = ver;
  memcpy(&buf[1], data, datasz);
  if (!double_sha256(hash, buf, datasz + 1)) {
    *b58c_sz = 0;
    goto loc_exit;
  }

  retval = base58_encode(buf, 1 + datasz + 4, b58c, b58c_sz);

loc_exit:
  sqlite3_free(buf);
  return retval;
}
#else
bool base58check_encode(const void *data, size_t datasz, uint8_t ver, char *b58c, size_t *b58c_sz){
  uint8_t buf[1 + datasz + 0x20];
  uint8_t *hash = &buf[1 + datasz];

  buf[0] = ver;
  memcpy(&buf[1], data, datasz);
  if (!double_sha256(hash, buf, datasz + 1)) {
    *b58c_sz = 0;
    return false;
  }

  return base58_encode(buf, 1 + datasz + 4, b58c, b58c_sz);
}
#endif
