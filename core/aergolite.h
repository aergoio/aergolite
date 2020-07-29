#include <stdbool.h>
#include "secp256k1-vrf.h"
#include "single_instance.h"

#ifndef AERGOLITE_API
#define AERGOLITE_API
#endif

typedef unsigned char uchar;


#define AERGOLITE_USE_SHORT_FILENAMES


typedef struct aergolite aergolite;

typedef struct aergolite_plugin aergolite_plugin;


typedef struct nodeauth nodeauth;

struct nodeauth {
  struct nodeauth *next;
  char pk[36];
  int  pklen;
  int  node_id;
  BOOL is_full_node;
  int64 last_nonce;
  int64 saved_nonce;
  void *log;
};


/*
typedef struct blob blob;

struct blob {
  char *ptr;
  int  size;
};

struct block {
  struct block *next;
  blob header;
  blob body;
  blob votes;
};
*/

/*
** The blobs are returned as binn buffers. They have the size on them,
** which can retrieved using binn_size()
*/
struct block {
  struct block *next;
  int64 height;
  int txn_count;
  void *header;
  void *body;
  void *votes;
  int  header_id;
  int  body_id;
  BOOL verify_ok;
#ifdef AERGOLITE_AMALGAMATION
  unsigned char id[32];
  unsigned char vrf_proof[81];
  unsigned char vrf_output[32];
  unsigned int  wait_time;
  int  num_votes;
  int  downloading_txns;
#endif
};


/*
** These constants are also present on the sqlite3.h
*/

#define HEADER_DATA     0x11
#define HEADER_SIG      0x12

#define BODY_MOD_PAGES  0x21
#define BODY_TXN_IDS    0x22


/*
** Internal constants
*/

#define TXN_PUBKEY      1
#define TXN_SIGNATURE   2
#define TXN_DATETIME    3
#define TXN_SQL         4



struct aergolite {
  int id;                     /* Node id */

  char *uri;                  /* the URI filename with parameters */
  char *node_info;            /* User defined information about this node */

  BOOL has_privkey;           /* if the private key for this node is available */
  unsigned char privkey[32];  /* the private key for this node */
  char pubkey[36];            /* the public key for this node */
  size_t pklen;               /* public key length */
  char admin_pubkey[36];      /* the blockchain admin public key */
  size_t admin_pklen;         /* the blockchain admin public key length */
  BOOL use_ledger;            /* if this node should communicate with a local Ledger Nano S */
  secp256k1_context *ecdsa;   /* the ECDSA context */
  secp256k1_pubkey pubkey_obj; /* the public key object */

  aergolite_plugin *plugin_functions; /* Which plugin is being used */
  void *plugin_instance;      /* The instance of the plugin related to this db connection */

  single_instance_handle single_instance; /* store the handle for the single instance */

  sqlite3 *main_db1;          /* database connection for 'local' data - used by the app */
  sqlite3 *main_db2;          /* database connection for 'local' data - used by the worker thread */
  sqlite3 *worker_db;         /* database connection for 'remote' data */
  Pager *main_pager1;         /* pager for 'local' data - used by the app */
  Pager *main_pager2;         /* pager for 'local' data - used by the worker thread */
  Pager *worker_pager;        /* pager for 'remote' data */

  sqlite3 *state_db;          /* database connection for the state database - used by the worker thread */

  BOOL dbWasEmpty;            /* if the db was empty when open */
  char *extensions;           /* the SQLite extensions to be loaded */

  nodeauth *authorizations;   /* List of node authorizations */

  BOOL is_full_node;
  BOOL changed_node_type;

  int64 block_height;         /* ... */
  struct block active_block;  /* ... */
  void *txn_ids;              /* The transactions on a specific block */
  void *mod_pages;            /* The modified pages on a specific block */
  void *pages_hashes;         /* The list of pages hashes */

  int64 current_local_nonce;  /* the current transaction being logged in the wal-local */
  int64 last_local_nonce;     /* last transaction from this node, processed or not */
  int64 max_active_nonce;     /* last transaction currently present in the wal-local file */
  int64 last_processed_nonce; /* last processed transaction from this node */
  u32   last_sent_frame;      /* the WAL frame of the last sent transaction */
  int64 last_sent_nonce;      /* last transaction sent to the primary node */
  int   wal_consensus_empty;  /* the state of the wal-consensus file */

  binn *processed_local_txns; /* local transactions processed on the blockchain */
  void *current_log;          /* local transaction being re-executed on wal-rotation */

  BOOL   useSqliteRowids;     /* do not use node id in the rowids */

  BOOL   db_is_ready;         /* if the app can read and write from/to the database */
};


/* All the pointers bellow must be valid */
struct aergolite_plugin {
  aergolite_plugin *next;                /* next entry in the list */
  char name[64];                         /* name of the plugin */
  void* (*xInit)(aergolite*, char* uri); /* initializes a new plugin instance */
  void (*xEnd)(void*);                   /* terminates the instance */
  void (*xOnNewLocalTransaction)(void*,void*); /* on_new_local_transaction notification */
  char* (*xStatus)(void*, int extended); /* used to retrieve the protocol status */
  void (*xNodeInfo)(void*, char*);       /* node info changed */
  void (*xNodeList)(void*, void*);       /* used to retrieve the node list */
};

SQLITE_PRIVATE int aergolitePluginsInit();


void xrc4_basic_crypt(char *out, char *in, int len, char *key, int keylen);

#define xrc4_encrypt_inplace(a,b,c,d) xrc4_basic_crypt(a,a,b,c,d)
#define xrc4_decrypt_inplace(a,b,c,d) xrc4_basic_crypt(a,a,b,c,d)

SQLITE_PRIVATE Pager * getPager(sqlite3 *db, const char *name);
SQLITE_PRIVATE Pager * getPagerFromiDb(sqlite3 *db, int iDb);

SQLITE_PRIVATE int  disable_aergolite(Pager *pPager);

SQLITE_API void to_hex(char *source, int size, char *dest);
SQLITE_API void from_hex(char *source, int size, char *dest);

SQLITE_PRIVATE void aergoliteCheckUserCommand(sqlite3 *db, Vdbe *p, char *zTrace);
SQLITE_PRIVATE void aergoliteProcessUserCommand(sqlite3 *db, int iDb, Pager *pPager, char *zSql);
SQLITE_PRIVATE int  aergoliteCheckExecResult(sqlite3 *db, int rc);
SQLITE_PRIVATE void aergoliteTransactionFailed(Pager *pPager);
SQLITE_PRIVATE void aergoliteDiscardTransaction(Pager *pPager);

SQLITE_PRIVATE void aergoliteSetSqlCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE void aergoliteDiscardLastCommand(sqlite3 *db);
SQLITE_PRIVATE int  aergoliteStoreCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE int  aergoliteStoreLastCommand(Pager *pPager);
SQLITE_PRIVATE int  aergoliteStoreTransactionPubkey(Pager *pPager);
SQLITE_PRIVATE int  aergoliteStoreTransactionNonce(Pager *pPager);
SQLITE_PRIVATE int  aergoliteStoreTransactionTime(Pager *pPager);
SQLITE_PRIVATE int  aergoliteCheckSignTransaction(Pager *pPager);

SQLITE_PRIVATE int  sign_txn_by_user(aergolite *this_node, binn *log);
SQLITE_PRIVATE int  sign_txn_by_node(aergolite *this_node, binn *log);

SQLITE_PRIVATE int  get_ledger_public_key(aergolite *this_node, char *pubkey);

SQLITE_PRIVATE void add_local_txn_status(aergolite *this_node, int64 nonce, char *status);
SQLITE_PRIVATE BOOL is_special_transaction(binn *log);
SQLITE_PRIVATE int  save_local_txn(aergolite *this_node, binn *log);
SQLITE_PRIVATE void process_new_local_transaction(aergolite *this_node, Pager *pPager);

SQLITE_PRIVATE int  lock_main_db(aergolite *this_node);
SQLITE_PRIVATE void unlock_main_db(aergolite *this_node);
SQLITE_PRIVATE int  aergolite_prepare_db(aergolite *this_node);

SQLITE_PRIVATE void invalidate_loaded_schemas(aergolite *this_node, BOOL on_main_db, BOOL on_worker_db);
SQLITE_PRIVATE int  check_for_log_rotation(aergolite *this_node);

SQLITE_PRIVATE sqlite_int64 aergoliteBuildRowId(int node_id, u32 seq_num);
SQLITE_PRIVATE int  aergoliteNodeIdFromRowId(sqlite_int64 value);
SQLITE_PRIVATE u32  aergoliteSeqFromRowId(sqlite_int64 value);

SQLITE_PRIVATE int64 get_last_processed_nonce(aergolite *this_node);

SQLITE_PRIVATE int  open_detached_worker_db(aergolite *this_node, sqlite3 **pworker_db);
SQLITE_PRIVATE int  open_main_db_connection2(aergolite *this_node);
SQLITE_PRIVATE int  open_worker_db(aergolite *this_node);

SQLITE_PRIVATE BOOL is_valid_node_type(char *type);
SQLITE_PRIVATE int  read_node_authorization(char *sql, char *pubkey, int *ppklen, char *type);
AERGOLITE_API  int  read_authorized_pubkey(void *log, char *pubkey, int *ppklen, char *type);

SQLITE_PRIVATE int  read_pragma_node_type(
  aergolite *this_node, char *sql, char *type, void **pnode_list, bool *pmake_default
);

SQLITE_PRIVATE int  pragma_add_node(Pager *pPager, char *zRight, Parse *pParse);
SQLITE_PRIVATE int  pragma_node_type(Pager *pPager, char *zRight, Parse *pParse);


SQLITE_PRIVATE int add_node_command(
  aergolite *this_node,
  int op,
  int node_id,
  int64 nonce,
  char *pubkey,
  int pklen
);
SQLITE_PRIVATE int  process_node_commands(aergolite *this_node, void *node_commands);

SQLITE_PRIVATE int  update_node_type(aergolite *this_node, int node_id, char *type);
SQLITE_PRIVATE void update_auth_type(aergolite *this_node, int node_id, char *type);

SQLITE_PRIVATE int  update_node_last_nonce(aergolite *this_node, int node_id, int64 nonce);
SQLITE_PRIVATE void update_auth_last_nonce(aergolite *this_node, int node_id, int64 nonce);

SQLITE_PRIVATE void update_authorizations(aergolite *this_node);
SQLITE_PRIVATE void update_auth_types(aergolite *this_node);

SQLITE_PRIVATE void save_auth_nonces(aergolite *this_node);
SQLITE_PRIVATE void reload_auth_nonces(aergolite *this_node);

AERGOLITE_API int is_node_authorized(aergolite *this_node, char *pubkey, int pklen, int *pnode_id);

AERGOLITE_API int aergolite_verify_authorization(
  aergolite *this_node,
  void *log
);

AERGOLITE_API int aergolite_get_authorization(
  aergolite *this_node,
  int node_id,
  char *pubkey,
  int *ppklen,
  void **pauthorization,
  BOOL *pis_full_node,
  int64 *plast_nonce
);

AERGOLITE_API int aergolite_insert_allowed_node(
  aergolite *this_node,
  int node_id,
  char *pubkey,
  int pklen,
  void *authorization,
  char *type,
  int64 last_nonce
);

AERGOLITE_API int aergolite_get_allowed_node(
  aergolite *this_node,
  int node_id,
  char *pubkey,
  int *ppklen,
  void **pauthorization,
  char *type,
  int64 *plast_nonce
);

typedef void (*on_allowed_node_cb)(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  void *authorization,
  char *type,
  int64 last_nonce
);

AERGOLITE_API int aergolite_iterate_allowed_nodes(
  aergolite *this_node,
  on_allowed_node_cb cb,
  void *arg
);


AERGOLITE_API int aergolite_load_current_state(
  aergolite *this_node,
  int64 *pblock_height,
  void **pheader,
  void **pbody,
  void **pvotes
);

SQLITE_PRIVATE int check_page(aergolite *this_node, Pgno pgno, void *data, int size);

SQLITE_PRIVATE int read_block_header(
  aergolite *this_node,
  void *header,
  int64 *ptimestamp,
  int64 *pblock_height,
  char **ptxns_hash,
  char **pstate_hash,
  void *id
);

SQLITE_PRIVATE int save_block(
  aergolite *this_node,
  struct block *block
);


SQLITE_PRIVATE int get_last_nonce_db(sqlite3 *db, int iDb, int64 *pnonce);
SQLITE_PRIVATE char * get_transaction_status_db(sqlite3 *db, int iDb, int64 nonce);

SQLITE_PRIVATE char * blockchain_status_json_db(sqlite3 *db, const char *name);
SQLITE_PRIVATE char * blockchain_status_json(Pager *pPager);
SQLITE_PRIVATE char * protocol_status_json_db(sqlite3 *db, const char *name, BOOL extended);
SQLITE_PRIVATE char * protocol_status_json(Pager *pPager, BOOL extended);

SQLITE_PRIVATE int set_current_node_info(sqlite3 *db, const char *name, char *info);
SQLITE_PRIVATE char * get_current_node_info(sqlite3 *db, const char *name);
