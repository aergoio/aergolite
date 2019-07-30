#ifndef SQLITE_OMIT_DISKIO
#ifdef SQLITE_HAS_CODEC

#include "xrc4.c"
#include "chacha.c"

#define CIPHER_XRC4    1
#define CIPHER_CHACHA  2

typedef struct _CRYPTKEY {
  u8        sbox[256];      // used with XRC4
  u8        key[32];        // used with ChaCha
} CRYPTKEY;

typedef struct _CRYPTBLOCK {
  Pager    *pPager;       /* Pager this cryptblock belongs to */
  int       dwPageSize;   /* Size of pages */
  void     *pBuf;         /* A buffer for encrypting/decrypting (if necessary) */
  int       iCipher;      /*  */
  int       iRounds;      /* Number of rounds in the ChaCha cipher */
  CRYPTKEY *hReadKey;     /* Key used to read from the database and write to the journal */
  CRYPTKEY *hWriteKey;    /* Key used to write to the database */
} CRYPTBLOCK;


#define SQLITE_HEADER_SIZE 100
#define CUSTOM_HEADER_LEN  16
#define IVLEN 8
#define plainpos 16
#define plainlen 8

#define MSGIVLEN 4


#if SQLITE_VERSION_NUMBER >= 3010000
#define CODEC_PAGER_GET(a,b,c) sqlite3PagerGet(a,b,c,0)
#else
#define CODEC_PAGER_GET(a,b,c) sqlite3PagerGet(a,b,c)
#endif

#if SQLITE_VERSION_NUMBER >= 3008007
#define CODEC_ROLLBACK(pbt) sqlite3BtreeRollback(pbt, SQLITE_OK, 0)
#elif (SQLITE_VERSION_NUMBER >= 3007011)
#define CODEC_ROLLBACK(pbt) sqlite3BtreeRollback(pbt, SQLITE_OK);
#else
#define CODEC_ROLLBACK(pbt) sqlite3BtreeRollback(pbt);
#endif

#if SQLITE_VERSION_NUMBER >= 3008007
#define CODEC_SET_ERROR(a,b,c) sqlite3ErrorWithMsg(a,b,c)
#else
#define CODEC_SET_ERROR(a,b,c) sqlite3Error(a,b,c)
#endif


/* Needed for re-keying */
static void * sqlite3pager_get_codecarg(Pager *pPager){
  return (pPager->xCodec) ? pPager->pCodec: NULL;
}

void sqlite3_activate_see(const char *info){
   /* not used */
}

/* Create a cryptographic context for a pager */
static CRYPTBLOCK * CreateCryptBlock(Pager *pager, CRYPTKEY *hKey){
  CRYPTBLOCK *pBlock;

  pBlock = sqlite3_malloc(sizeof(CRYPTBLOCK));
  if (!pBlock) return NULL;
  memset(pBlock, 0, sizeof(CRYPTBLOCK));

  pBlock->pPager = pager;
  pBlock->dwPageSize = (int)pager->pageSize;
  pBlock->iCipher = pager->iCipher;
  pBlock->iRounds = pager->iRounds;

  pBlock->hReadKey = hKey;
  pBlock->hWriteKey = hKey;

  pBlock->pBuf = sqlite3_malloc(pBlock->dwPageSize);
  if (!pBlock->pBuf) goto loc_failed;

  return pBlock;

loc_failed:
   sqlite3_free(pBlock);
   return NULL;
}

/* Destroy a cryptographic context and any buffers and keys allocated therein */
static void sqlite3CodecFree(void *pv){
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)pv;

  /* Destroy the read key if there is one */
  if( pBlock->hReadKey ){
    sqlite3_free(pBlock->hReadKey);
  }

  /* If there's a writekey and its not equal to the readkey, destroy it */
  if( pBlock->hWriteKey && pBlock->hWriteKey!=pBlock->hReadKey ){
    sqlite3_free(pBlock->hWriteKey);
  }

  /* If there's extra buffer space allocated, free it as well */
  if (pBlock->pBuf) {
    sqlite3_free(pBlock->pBuf);
  }

  /* All done with this cryptblock */
  sqlite3_free(pBlock);
}

void sqlite3CodecSizeChange(void *pArg, int pageSize, int reservedSize){
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)pArg;
  void *pTemp;

  if( !pBlock ) return;

  assert(reservedSize == IVLEN);

  if (pBlock->dwPageSize != pageSize){
    pBlock->dwPageSize = (int)pageSize;

    /* If there's extra buffer space allocated, free it as well */
    if (pBlock->pBuf) {
      pTemp = sqlite3_realloc(pBlock->pBuf, pBlock->dwPageSize);
      if (pTemp) pBlock->pBuf = pTemp;
    }
  }

}

#ifdef DEBUGREPLICA
void print_block(char * desc, unsigned char *data, int len) {
  int i, j;

  printf("%s: (%d bytes)\n", desc, len);

  for (i = 0; i < len; i++) {
    printf("%02x ", data[i]);
    if (((i + 1) % 16) == 0) {
      printf(" ");
      for (j = 0; j <= i; j++) {
        printf("%c", data[j]);
      }
      printf("\n");
      data += 16;
      len -= 16;
      i -= 16;
    }
  }

  for (j = 0; j < i; j++) {
    printf("%c", data[j]);
  }
  printf("\n");
}
#endif

void codec_encrypt(int iCipher, int iRounds, CRYPTKEY *hKey, int pgno, u8 *data, int size) {
  u8 iv[IVLEN];

  CODECTRACE("codec_encrypt pg=%d size=%d\n", pgno, size);

  /* generate a new random iv */
  sqlite3_randomness(IVLEN, iv);

#ifdef DEBUGREPLICA
  print_block("iv", iv, IVLEN);
#endif

  /* correct the data length removing the space for the iv from the buffer's end */
  size -= IVLEN;

  switch( iCipher ){
  case CIPHER_XRC4:
    CODECTRACE("using xrc4\n");
    xrc4_crypt(data, data, size, hKey->sbox, iv, IVLEN, pgno);
    break;
  case CIPHER_CHACHA:
    CODECTRACE("using chacha\n");
    chacha_encrypt(data, data, size, hKey->key, iv, IVLEN, pgno, iRounds);
    break;
  default:
    CODECTRACE("-- no cipher!\n");
  }

  /* save the nounce/iv at the buffer's end */
  data += size;
  memcpy(data, iv, IVLEN);

}

void codec_decrypt(int iCipher, int iRounds, CRYPTKEY *hKey, int pgno, u8 *data, int size) {
  u8 *iv;

  CODECTRACE("codec_decrypt pg=%d size=%d ... \n", pgno, size);

  /* get the nounce/iv from the buffer's end */
  size -= IVLEN;
  iv = data + size;

#ifdef DEBUGREPLICA
  print_block("iv", iv, IVLEN);
#endif

  switch( iCipher ){
  case CIPHER_XRC4:
    CODECTRACE("using xrc4\n");
    xrc4_crypt(data, data, size, hKey->sbox, iv, IVLEN, pgno);
    break;
  case CIPHER_CHACHA:
    CODECTRACE("using chacha\n");
    chacha_decrypt(data, data, size, hKey->key, iv, IVLEN, pgno, iRounds);
    break;
  default:
    CODECTRACE("-- no cipher!\n");
  }

  //if(pgno==1)
  //  print_block("data", data, size);

}

/* Encrypt/Decrypt functionality, called by pager.c */
/*
 * sqlite3Codec can be called in multiple modes.
 * encrypt mode - expected to return a pointer to the 
 *   encrypted data without altering pData.
 * decrypt mode - expected to return a pointer to pData, with
 *   the data decrypted in the input buffer
 */
void * sqlite3Codec(void *pArg, void *pdata, Pgno pgno, int mode){
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)pArg;
  unsigned char *data = (unsigned char *) pdata;
  int size = pBlock->dwPageSize;
  int offset=0;
#ifdef CODEC_ENCRYPT_DB_HEADER
  unsigned char plaindata[plainlen];
#else
  if (pgno==1) offset = SQLITE_HEADER_SIZE;
#endif

  if (!pBlock) return data;
  CODECTRACE("sqlite3Codec\n");
  assert(pBlock->pPager->nReserve == IVLEN);

#ifdef CODEC_ENCRYPT_DB_HEADER
  if (pgno==1) {
    /* Make a copy of the data that will not be encrypted */
    memcpy(plaindata, &data[plainpos], plainlen);
  }
#endif

  switch(mode){
  case 0: /* Undo a "case 7" journal file encryption */
  case 2: /* Reload a page */
  case 3: /* Load a page */
    if (!pBlock->hReadKey) break;

    /* Decrypt the block */
    codec_decrypt(pBlock->iCipher, pBlock->iRounds, pBlock->hReadKey, pgno, data + offset, size - offset);

#if CODEC_USE_CUSTOM_HEADER
  if (pgno==1) {
    /* Restore the original header string */
    memcpy(data, SQLITE_FILE_HEADER, CUSTOM_HEADER_LEN);
  }
#endif

    break;
  case 6: /* Encrypt a page for the main database file */
    if (!pBlock->hWriteKey) break;

    /* Encrypt the block with the Write key */
    memcpy(pBlock->pBuf, data, size);
    data = pBlock->pBuf;
    codec_encrypt(pBlock->iCipher, pBlock->iRounds, pBlock->hWriteKey, pgno, data + offset, size - offset);

#if CODEC_USE_CUSTOM_HEADER
  if (pgno==1) {
    /* Store the header custom string */
    memcpy(data, CODEC_CUSTOM_HEADER, CUSTOM_HEADER_LEN);
  }
#endif

    break;
  case 7: /* Encrypt a page for the journal file */
    /* Under normal circumstances, the readkey is the same as the writekey.  However,
    when the database is being rekeyed, the readkey is not the same as the writekey.
    The rollback journal must be written using the original key for the
    database file because it is, by nature, a rollback journal.
    Therefore, for case 7, when the rollback is being written, always encrypt using
    the database's readkey, which is guaranteed to be the same key that was used to
    read the original data.
    */
    if (!pBlock->hReadKey) break;

    /* Encrypt the block with the Read key */
    memcpy(pBlock->pBuf, data, size);
    data = pBlock->pBuf;
    codec_encrypt(pBlock->iCipher, pBlock->iRounds, pBlock->hReadKey, pgno, data + offset, size - offset);

#if CODEC_USE_CUSTOM_HEADER
  if (pgno==1) {
    /* Store the header custom string */
    memcpy(data, CODEC_CUSTOM_HEADER, CUSTOM_HEADER_LEN);
  }
#endif

    break;
  }

#ifdef CODEC_ENCRYPT_DB_HEADER
  if (pgno==1) {
    /* Restore the plain data */
    memcpy(&data[plainpos], plaindata, plainlen);
  }
#endif

  return data;
}

/* Derive an encryption key from a user-supplied buffer */
static CRYPTKEY * DeriveKey(Pager *pPager, const u8 *pKey, int nKeyLen, char **pErrMsg){
  CRYPTKEY *hKey;

  if( !pKey || nKeyLen==0 ) return NULL;

  hKey = sqlite3_malloc(sizeof(CRYPTKEY));
  if( !hKey ) {
    *pErrMsg = "no memory";
    return (CRYPTKEY*)-1;
  }
  memset(hKey, 0, sizeof(CRYPTKEY));

  switch( pPager->iCipher ){
  case CIPHER_XRC4:
    xrc4_init(pKey, nKeyLen, hKey->sbox);
    break;
  case CIPHER_CHACHA:
    if( nKeyLen!=32 ) {
      //sqlite3_log(SQLITE_MISUSE, "the key must have 32 bytes. the supplied has %d bytes", nKeyLen);
      *pErrMsg = "The key for ChaCha must have 32 bytes";
      goto loc_failed;
    }
    memcpy(hKey->key, pKey, 32);
    break;
  default:
    //sqlite3_log(SQLITE_MISUSE, "use the 'PRAGMA cipher=...' command before");
    *pErrMsg = "The cipher was not set. Use 'cipher=...' on URI or the 'PRAGMA cipher=...' command";
    goto loc_failed;
  }

  return hKey;
loc_failed:
  sqlite3_free(hKey);
  return (CRYPTKEY*)-1;
}

/* Duplicate the supplied key */
static CRYPTKEY * DuplicateKey(CRYPTKEY *hSourceKey){
  CRYPTKEY *hKey;
  if( !hSourceKey ) return NULL;
  hKey = sqlite3_malloc(sizeof(CRYPTKEY));
  if( !hKey ) return NULL;
  memcpy(hKey, hSourceKey, sizeof(CRYPTKEY));
  return hKey;
}

/* Handles the "PRAGMA cipher" command, storing the cipher in the pager */
SQLITE_API char *sqlite3_set_cipher(
  sqlite3 *db,                   /* Database connection */
  const char *zDbName,           /* Name of the database */
  const char *pCipher            /* The cipher name */
){
  Pager *pPager;
  CRYPTBLOCK *pBlock;

  pPager = getPager(db, zDbName);
  if( !pPager ) return "error";
  pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPager);

  /* if the database is encrypted we get the active cipher */
  if( pBlock ){
    return "the database is already encrypted";
  }

  if( sqlite3_stricmp(pCipher, "xrc4")==0 ){
    pPager->iCipher = CIPHER_XRC4;
  } else if( sqlite3_strnicmp(pCipher, "chacha", 6)==0 ){
    if( sqlite3GetInt32(pCipher + 6, &pPager->iRounds)==0 )
      return "you must inform the number of rounds. eg: chacha8";
    if( pPager->iRounds<=0 || (pPager->iRounds&1)==1 )
      return "the number of rounds must be a positive even number";
    pPager->iCipher = CIPHER_CHACHA;
  } else {
    return "cipher not recognized";
  }

  return 0;
}

/* Retrieves the value for the "PRAGMA cipher" command */
SQLITE_API int sqlite3_get_cipher(
  sqlite3 *db,                   /* Database connection */
  const char *zDbName,           /* Name of the database */
  char *pCipher                  /* The cipher name */
){
  Pager *pPager;
  CRYPTBLOCK *pBlock;
  int iCipher, iRounds;

  pPager = getPager(db, zDbName);
  if( !pPager ) return 0;
  pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPager);

  /* if the database is encrypted we get the active cipher */
  if( pBlock ){
    iCipher = pBlock->iCipher;
    iRounds = pBlock->iRounds;
  } else {
  /* otherwise get the last cipher set in the pager */
    iCipher = pPager->iCipher;
    iRounds = pPager->iRounds;
  }

  switch( iCipher ){
  case CIPHER_XRC4:
    strcpy(pCipher, "xrc4");
    break;
  case CIPHER_CHACHA:
    sprintf(pCipher, "chacha%d", iRounds);
    break;
  default:
    return 0;
  }

  return 1;
}

/* Set the number of reserved bytes at the end of each page */
static int codec_set_reserved_bytes(sqlite3 *db, int nDb, int nPageSize, int nReserve) {
  Btree *pBt = db->aDb[nDb].pBt;
  int rc;
  CODECTRACE("codec_set_reserved_bytes: pagesize=%d reserve=%d\n", nPageSize, nReserve);
  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3BtreeSetPageSize(pBt, nPageSize, nReserve, 0);  //  1);  iFix must be 1 if clearing BTS_PAGESIZE_FIXED
  sqlite3_mutex_leave(db->mutex);
  return rc;
}

/* Called by sqlite and sqlite3_key to attach a key to a database. */
int sqlite3CodecAttach(sqlite3 *db, int nDb, const void *pKey, int nKeyLen){
  Pager *pPager = sqlite3BtreePager(db->aDb[nDb].pBt);
  CRYPTKEY *hKey = 0;
  int rc = SQLITE_ERROR;

  CODECTRACE("sqlite3CodecAttach key=%s\n", (char*)pKey);

  /* No key specified, could mean either use the main db's encryption or no encryption */
  if( !pKey || !nKeyLen ){
    if( !nDb ){
      return SQLITE_OK; /* Main database, no key specified so not encrypted */
    } else {
      /* Attached database, use the main database's key */
      /* Get the encryption block for the main database and attempt to duplicate the key
      ** for use by the attached database
      */
      Pager *pPagerMain = sqlite3BtreePager(db->aDb[0].pBt);
      CRYPTBLOCK *pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPagerMain);

      if (!pBlock) return SQLITE_OK; /* Main database is not encrypted so neither will be any attached database */
      if (!pBlock->hReadKey) return SQLITE_OK; /* Not encrypted */

      if (!(hKey = DuplicateKey(pBlock->hReadKey))){
        return rc; /* Unable to duplicate the key */
      }
    }
  } else {
    /* User-supplied passphrase, so create a cryptographic key out of it */
    char *errmsg;
    hKey = DeriveKey(pPager, (u8*)pKey, nKeyLen, &errmsg);
    if( hKey==(CRYPTKEY *)-1 ){
      CODEC_SET_ERROR(db, rc, errmsg);
      return rc;
    }
  }

  /* Create a new encryption block and assign the codec to the new attached database */
  CODECTRACE("sqlite3CodecAttach hKey=%p\n", hKey);
  if( hKey ){
    CRYPTBLOCK *pBlock;
    CODECTRACE("setting the codec\n");
    if( pPager->nReserve!=IVLEN ){
      rc = codec_set_reserved_bytes(db, nDb, pPager->pageSize, IVLEN);
      if( rc ) goto loc_failed;
    }
    pBlock = CreateCryptBlock(pPager, hKey);
    if (!pBlock) { rc = SQLITE_NOMEM; goto loc_failed; }
    sqlite3PagerSetCodec(pPager, sqlite3Codec, sqlite3CodecSizeChange, sqlite3CodecFree, pBlock);
    rc = SQLITE_OK;
  }

  return rc;
loc_failed:
  CODECTRACE("failed: setting the codec");
  sqlite3_free(hKey);
  return rc;
}

/* Copy the codec info from one pager to another */
int sqlite3CodecCopy(sqlite3 *db, int nDb, Pager *pPager, Pager *pFromPager){
  int rc;
  CRYPTKEY *hKey=NULL;
  CRYPTBLOCK *pBlockFrom = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pFromPager);

  CODECTRACE("sqlite3CodecCopy pBlockFrom=%p\n", pBlockFrom);

  if( !pBlockFrom ) return SQLITE_OK;           /* Not encrypted */
  if( !pBlockFrom->hReadKey ) return SQLITE_OK; /* Not encrypted */

  if( pPager->nReserve!=IVLEN ){
    rc = codec_set_reserved_bytes(db, nDb, pPager->pageSize, IVLEN);
    if( rc ) goto loc_failed;
  }

  hKey = DuplicateKey(pBlockFrom->hReadKey);

  CODECTRACE("sqlite3CodecCopy hKey=%p\n", hKey);

  if( !hKey ){
    rc = SQLITE_NOMEM;
  } else {
    /* Create a new encryption block and assign the codec to the pager */
    CRYPTBLOCK *pBlock = CreateCryptBlock(pPager, hKey);
    if( !pBlock ) { rc = SQLITE_NOMEM; goto loc_failed; }
    sqlite3PagerSetCodec(pPager, sqlite3Codec, sqlite3CodecSizeChange, sqlite3CodecFree, pBlock);
    rc = SQLITE_OK;
  }

  return rc;
loc_failed:
  CODECTRACE("failed: setting the codec");
  if( hKey ) sqlite3_free(hKey);
  return rc;
}

/* Once a password has been supplied and a key created, we don't keep the
** original password for security purposes.  Therefore return NULL.
*/
void sqlite3CodecGetKey(sqlite3 *db, int nDb, void **ppKey, int *pnKeyLen) {
  Btree *pBt = db->aDb[nDb].pBt;
  Pager *p = sqlite3BtreePager(pBt);
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(p);
  if (ppKey) *ppKey = 0;
  if (pnKeyLen) *pnKeyLen = pBlock ? 1: 0;
}

/* We do not attach this key to the temp store, only the main database. */
SQLITE_API int sqlite3_key_v2(sqlite3 *db, const char *zDbName, const void *pKey, int nKey){
  int nDb = getDbFromName(db, zDbName);
  return sqlite3CodecAttach(db, nDb, pKey, nKey);
}

SQLITE_API int sqlite3_key(sqlite3 *db, const void *pKey, int nKey){
  return sqlite3_key_v2(db, 0, pKey, nKey);
}

/* Changes the encryption key for an existing database. */
SQLITE_API int sqlite3_rekey_v2(sqlite3 *db, const char *zDbName, const void *pKey, int nKeySize){
  int nDb = getDbFromName(db, zDbName);
  Btree *pBt = db->aDb[nDb].pBt;  //Btree *pBt = sqlite3DbNameToBtree(db, zDbName);
  Pager *pPager = sqlite3BtreePager(pBt);
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPager);
  CRYPTKEY *hKey;
  char *errmsg;
  int rc = SQLITE_ERROR;

  CODECTRACE("sqlite3_rekey_v2");

  hKey = DeriveKey(pPager, (u8*)pKey, nKeySize, &errmsg);
  if (hKey == (CRYPTKEY*)-1) {
    CODEC_SET_ERROR(db, rc, errmsg);
    return rc;
  }

  if (!pBlock && !hKey) return SQLITE_OK; /* Wasn't encrypted to begin with */

  /* To rekey a database, we change the writekey for the pager.  The readkey remains
  ** the same
  */
  if (!pBlock) { /* Encrypt an unencrypted database */
    if( pPager->nReserve!=IVLEN ) return SQLITE_ERROR;
    pBlock = CreateCryptBlock(pPager, hKey);
    if (!pBlock) return SQLITE_NOMEM;
    pBlock->hReadKey = 0; /* Original database is not encrypted */
    sqlite3PagerSetCodec(pPager, sqlite3Codec, sqlite3CodecSizeChange, sqlite3CodecFree, pBlock);
  } else { /* Change the writekey for an already-encrypted database */
    pBlock->hWriteKey = hKey;
  }

  sqlite3_mutex_enter(db->mutex);

  /* Start a transaction */
  rc = sqlite3BtreeBeginTrans(pBt, 1, 0);

  if( !rc ){
    /* Rewrite all the pages in the database using the new encryption key */
    int nPage;
    Pgno nSkip = PAGER_MJ_PGNO(pPager);
    DbPage *pPage;
    Pgno n;

    sqlite3PagerPagecount(pPager, &nPage);

    for(n=1; rc==SQLITE_OK && n<=(Pgno)nPage; n++){
      if (n == nSkip) continue;
      rc = CODEC_PAGER_GET(pPager, n, &pPage);
      if( !rc ){
        rc = sqlite3PagerWrite(pPage);
        sqlite3PagerUnref(pPage);
      }
    }
  }

  /* If we succeeded, try to commit the transaction */
  if( !rc ){
    rc = sqlite3BtreeCommit(pBt);
  }

  /* If we failed, rollback */
  if( rc ){
    CODEC_ROLLBACK(pBt);
  }

  /* If we succeeded, destroy any previous read key this database used
  ** and make the readkey equal to the writekey
  */
  if( !rc ){
    if (pBlock->hReadKey) {
      sqlite3_free(pBlock->hReadKey);
    }
    pBlock->hReadKey = pBlock->hWriteKey;
  } else {
    /* We failed.  Destroy the new writekey (if there was one) and revert it back to
    ** the original readkey
    */
    if (pBlock->hWriteKey) {
      sqlite3_free(pBlock->hWriteKey);
    }
    pBlock->hWriteKey = pBlock->hReadKey;
  }

  /* If the readkey and writekey are both empty, there's no need for a codec on this
  ** pager anymore.  Destroy the crypt block and remove the codec from the pager.
  ** The database will still keep the reserved bytes at the end of each page.
  */
  if (!pBlock->hReadKey && !pBlock->hWriteKey){
    sqlite3PagerSetCodec(pPager, NULL, NULL, NULL, NULL);
  }

  sqlite3_mutex_leave(db->mutex);

  return rc;
}

SQLITE_API int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey){
  return sqlite3_rekey_v2(db, 0, pKey, nKey);
}

/* Encrypt a message if a codec is used */
u8 * aergolite_msg_encrypt(Pager *pPager, u8 *data, int *psize, int counter) {
  int size;
  u8 *data2, *dest, *iv;
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPager);

  CODECTRACE("msg_encrypt pBlock=%p \n", pBlock);
  if( !pBlock ) return data;

  size = *psize;
  CODECTRACE("msg_encrypt data=%p size=%d counter=%d\n", data, size, counter);

  data2 = sqlite3_malloc(size + MSGIVLEN);
  if( !data2 ) return NULL;

  /* generate a new random iv */
  sqlite3_randomness(MSGIVLEN, data2);

  /* set the pointers */
  dest = data2 + MSGIVLEN;
  iv = data2;

  switch( pBlock->iCipher ){
  case CIPHER_XRC4:
    CODECTRACE("using xrc4\n");
    xrc4_crypt(dest, data, size, pBlock->hReadKey->sbox, iv, MSGIVLEN, counter);
    break;
  case CIPHER_CHACHA:
    CODECTRACE("using chacha\n");
    chacha_encrypt(dest, data, size, pBlock->hReadKey->key, iv, MSGIVLEN, counter, pBlock->iRounds);
    break;
  default:
    CODECTRACE("-- no cipher!\n");
  }

  /* correct the data length adding the space for the iv */
  size += MSGIVLEN;
  *psize = size;

  /* return the data */
  return data2;
}

/* Decrypt a message if a codec is used */
u8 * aergolite_msg_decrypt(Pager *pPager, u8 *data, int *psize, int counter) {
  int size;
  u8 *dest, *iv;
  CRYPTBLOCK *pBlock = (CRYPTBLOCK*)sqlite3pager_get_codecarg(pPager);

  CODECTRACE("msg_decrypt pBlock=%p \n", pBlock);
  if( !pBlock ) return data;

  size = *psize;
  CODECTRACE("msg_decrypt data=%p size=%d counter=%d\n", data, size, counter);

  /* set the pointers */
  iv = data;
  data += MSGIVLEN;
  size -= MSGIVLEN;
  dest = data;

  switch( pBlock->iCipher ){
  case CIPHER_XRC4:
    CODECTRACE("using xrc4\n");
    xrc4_crypt(dest, data, size, pBlock->hReadKey->sbox, iv, MSGIVLEN, counter);
    break;
  case CIPHER_CHACHA:
    CODECTRACE("using chacha\n");
    chacha_decrypt(dest, data, size, pBlock->hReadKey->key, iv, MSGIVLEN, counter, pBlock->iRounds);
    break;
  default:
    CODECTRACE("-- no cipher!\n");
  }

  /* correct the data length removing the space for the iv */
  *psize = size;

  /* return the data */
  return dest;
}

#endif /* SQLITE_HAS_CODEC */
#endif /* SQLITE_OMIT_DISKIO */
