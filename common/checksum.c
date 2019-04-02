
/***************************************************************************/
// crc32
/***************************************************************************/

// ATENTION: these functions must be static otherwise a segmentation fault occurs
//           with the node-gyp compilation, probably calling a function with the
//           same name from another file

unsigned int   crc_table[256];
BOOL           crc_table_initialized = FALSE;

static void crc32_init() {
  unsigned int crc;
  int x, y;

  if (crc_table_initialized == TRUE) return;

  for (x = 0; x < 256; x++) {
      crc = x;
      for (y = 8; y > 0; y--) {
         if (crc & 1)
            crc = (crc >> 1) ^ 0xEDB88320;
         else
            crc >>= 1;
      }
      crc_table[x] = crc;
  }

  crc_table_initialized = TRUE;

}

static unsigned int crc32(char *bufptr, int buflen) {
  unsigned int  crc;
  unsigned char c;
  int   i;

  if ((bufptr == 0) || (buflen <= 0)) return 0;
  crc32_init();

  crc = 0xFFFFFFFF;

  for(i=0; i < buflen; i++){
    c = bufptr[i];
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_table[(crc ^ c) & 0xFF];
  }

  crc^=0xFFFFFFFF;

  return crc;
}

static unsigned int crc32rev(char *bufptr, int buflen) {
  unsigned int  crc;
  unsigned char c;
  int   i;

  if ((bufptr == 0) || (buflen <= 0)) return 0;
  crc32_init();

  crc = 0xFFFFFFFF;

  for(i = buflen - 1; i >= 0; i--){
    c = bufptr[i];
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_table[(crc ^ c) & 0xFF];
  }

  crc^=0xFFFFFFFF;

  return crc;
}
