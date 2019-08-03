
#include "single_instance.h"

typedef unsigned char uchar;


#define AERGOLITE_USE_SHORT_FILENAMES


typedef struct aergolite aergolite;

typedef struct aergolite_plugin aergolite_plugin;


struct transaction {
  struct transaction *next;
  int64 seq;
  int node_id;
  int64 tid;
  void *log;
  int64 prev_tid;
  u8    hash[32];
  int ack_count;              /* Number of nodes that acknowledged the sent transaction */
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
  blob signatures;
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
  void *signatures;
};


/*
** These constants are also present on the sqlite3.h
*/

#define HEADER_DATA     0x11
#define HEADER_SIG      0x12

#define BODY_MOD_PAGES  0x21
#define BODY_TXN_IDS    0x22



struct aergolite {
  int id;                     /* Node id */

  char *uri;                  /* the URI filename with parameters */
  char *node_info;            /* User defined information about this node */

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

  int64 block_height;         /* ... */
  struct block current_block; /* ... */
  void *txn_ids;              /* The transactions on a specific block */
  void *mod_pages;            /* The modified pages on a specific block */
  void *pages_hashes;         /* The list of pages hashes */

  int64 current_local_nonce;  /* the current transaction being logged in the wal-local */
//int64 current_remote_nonce; /* the current transaction being logged in the wal-remote, if not using log table */
  int64 last_local_nonce;     /* last transaction in the wal-local file */
  int64 last_remote_nonce;    /* last transaction in the blockchain */
  u32 last_sent_frame;        /* the WAL frame of the last sent transaction */
  int64 last_sent_nonce;      /* last transaction sent to the primary node */

//int    num_local_txns;      /* number of transactions on the wal-local */
//int    num_blockchain_txns; /* number of transactions on the blockchain */

  BOOL   useSqliteRowids;     /* do not use node id in the rowids */

  BOOL   db_is_ready;         /* if the app can read and write from/to the database */
  //u32    total_pages;         /* to calculate the download progress */
  //u32    downloaded_pages;    /* to calculate the download progress */
  //int    db_state;
  //int    sync_down_state;     /* (sync downstream state) UNKNOWN, SYNCING, IN_SYNC, OUTDATED */
  //int    sync_up_state;       /* (sync upstream state)   HAS_LOCAL_CHANGES, SYNCING, IN_SYNC */
};


/* All the pointers bellow must be valid */
struct aergolite_plugin {
  aergolite_plugin *next;                /* next entry in the list */
  char name[64];                         /* name of the plugin */
  void* (*xInit)(aergolite*, char* uri); /* initializes a new plugin instance */
  void (*xEnd)(void*);                   /* terminates the instance */
  void (*xOnNewLocalTransaction)(void*); /* on_new_local_transaction notification */
  char* (*xStatus)(void*, int extended); /* used to retrieve the protocol status */
};

SQLITE_PRIVATE int aergolitePluginsInit();


SQLITE_PRIVATE Pager * getPager(sqlite3 *db, const char *name);
SQLITE_PRIVATE Pager * getPagerFromiDb(sqlite3 *db, int iDb);

SQLITE_PRIVATE int  disable_aergolite(Pager *pPager);

SQLITE_PRIVATE void aergoliteCheckUserCommand(sqlite3 *db, Vdbe *p, char *zTrace);
SQLITE_PRIVATE void aergoliteProcessUserCommand(sqlite3 *db, int iDb, Pager *pPager, char *zSql);
SQLITE_PRIVATE void aergoliteCheckUserCmdResponse(sqlite3 *db, int rc);
//SQLITE_PRIVATE int  aergoliteAddSqlCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE int  aergoliteSaveSession(Pager *pPager);
SQLITE_PRIVATE void aergoliteDiscardLog(Pager *pPager);

SQLITE_PRIVATE void aergoliteSetSqlCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE void aergoliteDiscardLastCommand(sqlite3 *db);
SQLITE_PRIVATE int  aergoliteStoreLastCommand(Pager *pPager);
SQLITE_PRIVATE int  aergoliteStoreLogTransactionNonce(Pager *pPager);
SQLITE_PRIVATE void aergoliteStoreLogTransactionTime(Pager *pPager);

SQLITE_PRIVATE sqlite_int64 aergoliteBuildRowId(int node_id, u32 seq_num);
SQLITE_PRIVATE int  aergoliteNodeIdFromRowId(sqlite_int64 value);
SQLITE_PRIVATE u32  aergoliteSeqFromRowId(sqlite_int64 value);

SQLITE_PRIVATE int64 get_last_remote_nonce(aergolite *this_node);

SQLITE_PRIVATE int  open_detached_worker_db(aergolite *this_node, sqlite3 **pworker_db);
SQLITE_PRIVATE int  open_main_db_connection2(aergolite *this_node);
SQLITE_PRIVATE int  open_worker_db(aergolite *this_node);


SQLITE_PRIVATE int add_node_command(
  aergolite *this_node,
  int op,
  int node_id,
  int64 nonce,
  char *pubkey,
  int pklen
);
SQLITE_PRIVATE int process_node_commands(aergolite *this_node, void *node_commands);

SQLITE_PRIVATE int update_node_last_nonce(
  aergolite *this_node,
  int node_id,
  int64 last_nonce
);

SQLITE_API int aergolite_insert_allowed_node(
  aergolite *this_node,
  int node_id,
  char *pubkey,
  int pklen,
  int64 last_nonce
);

SQLITE_API int aergolite_get_allowed_node(
  aergolite *this_node,
  int node_id,
  char *pubkey,
  int *ppklen,
  int64 *plast_nonce
);


SQLITE_PRIVATE int check_page(aergolite *this_node, Pgno pgno, void *data, int size);

SQLITE_PRIVATE int read_block_header(
  aergolite *this_node,
  void *header,
  int64 *ptimestamp,
  int64 *pblock_height,
  char **ptxns_hash,
  char **pstate_hash
);

SQLITE_PRIVATE int save_block(
  aergolite *this_node,
  int64 block_height,
  struct block *block
);


SQLITE_PRIVATE char * blockchain_status_json_db(sqlite3 *db, const char *name);
SQLITE_PRIVATE char * blockchain_status_json(Pager *pPager);
SQLITE_PRIVATE char * protocol_status_json_db(sqlite3 *db, const char *name, BOOL extended);
SQLITE_PRIVATE char * protocol_status_json(Pager *pPager, BOOL extended);

SQLITE_PRIVATE int set_current_node_info(sqlite3 *db, const char *name, char *info);
SQLITE_PRIVATE char * get_current_node_info(sqlite3 *db, const char *name);
