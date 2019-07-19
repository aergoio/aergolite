
#include "single_instance.h"

typedef unsigned char uchar;


#define LITESYNC_USE_SHORT_FILENAMES


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

  void *txn_ids;              /* The transactions on a specific block */
  void *mod_pages;            /* The modified pages on a specific block */
  void *pages_hashes;         /* The list of pages hashes */

  int64 current_local_tid;    /* the current transaction being logged in the wal-local */
  int64 current_remote_tid;   /* the current transaction being logged in the wal-remote, if not using log table */
  int64 last_local_tid;       /* last transaction in the wal-local file */
  int64 last_remote_tid;      /* last transaction in the blockchain */
  uchar last_blockchain_hash[32] ; /* last transaction hash */
  u32 last_sent_frame;        /* the WAL frame of the last sent transaction */
  int64 last_sent_tid;        /* last transaction sent to the primary node */
  int64 last_ack_tid;         /* last transaction acknowledged by the primary node */
  int64 last_valid_tid;       /* last transaction accepted by the primary node */
//int64 base_tid;             /* the 'base' transaction id, the one that comes before the first on the log table */
  int64 failed_txn_id;        /* failed transaction id */
  binn *failed_txns;          /* list of failed transactions */

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

SQLITE_PRIVATE int  disable_litesync(Pager *pPager);

SQLITE_PRIVATE void litesyncCheckUserCommand(sqlite3 *db, Vdbe *p, char *zTrace);
SQLITE_PRIVATE void litesyncProcessUserCommand(sqlite3 *db, int iDb, Pager *pPager, char *zSql);
SQLITE_PRIVATE void litesyncCheckUserCmdResponse(sqlite3 *db, int rc);
//SQLITE_PRIVATE int  litesyncAddSqlCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE int  litesyncSaveSession(Pager *pPager);
SQLITE_PRIVATE void litesyncDiscardLog(Pager *pPager);

SQLITE_PRIVATE void litesyncSetSqlCommand(Pager *pPager, char *sql);
SQLITE_PRIVATE void litesyncDiscardLastCommand(sqlite3 *db);
SQLITE_PRIVATE int  litesyncStoreLastCommand(Pager *pPager);
SQLITE_PRIVATE int  litesyncStoreLogTransactionId(Pager *pPager);
SQLITE_PRIVATE void litesyncStoreLogTransactionTime(Pager *pPager);

SQLITE_PRIVATE sqlite_int64 litesyncBuildRowId(int node_id, u32 seq_num);
SQLITE_PRIVATE int  litesyncNodeIdFromRowId(sqlite_int64 value);
SQLITE_PRIVATE u32  litesyncSeqFromRowId(sqlite_int64 value);

SQLITE_PRIVATE int   litesyncGetWalLog(Pager *pPager, u32 start, int64 tid, binn **plog);
SQLITE_PRIVATE int64 last_tid_from_wal_log(Pager *pPager);

SQLITE_PRIVATE int  open_detached_worker_db(aergolite *this_node, sqlite3 **pworker_db);
SQLITE_PRIVATE int  open_main_db_connection2(aergolite *this_node);
SQLITE_PRIVATE int  open_worker_db(aergolite *this_node);



SQLITE_PRIVATE int check_page(aergolite *this_node, Pgno pgno, void *data, int size);



SQLITE_PRIVATE int check_if_failed_txn(aergolite *this_node, int64 tid);

SQLITE_PRIVATE char * blockchain_status_json_db(sqlite3 *db, const char *name);
SQLITE_PRIVATE char * blockchain_status_json(Pager *pPager);
SQLITE_PRIVATE char * protocol_status_json_db(sqlite3 *db, const char *name, BOOL extended);
SQLITE_PRIVATE char * protocol_status_json(Pager *pPager, BOOL extended);

SQLITE_PRIVATE int set_current_node_info(sqlite3 *db, const char *name, char *info);
SQLITE_PRIVATE char * get_current_node_info(sqlite3 *db, const char *name);
