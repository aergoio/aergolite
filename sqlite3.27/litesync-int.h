
//#include <uv.h>
#include "../common/uv_msg_framing.c"
#include "../common/uv_send_message.c"
#include "../common/single_instance.h"

typedef unsigned char uchar;


/* worker thread commands */
#define WORKER_THREAD_NEW_TRANSACTION  0xcd01  /*  */
#define WORKER_THREAD_EXIT             0xcd02  /*  */
#define WORKER_THREAD_OK               0xcd03  /*  */


/* peer communication */

#define LITESYNC_CMD                 0x43292173

/* peer message commands */
#define LITESYNC_CMD_ID              0xcd01     /* peer identification */
//#define LITESYNC_REQUEST_NODE_ID     0xcd02     /* request a node id */
//#define LITESYNC_NEW_NODE_ID         0xcd03     /* send the new node id */
#define LITESYNC_ID_CONFLICT         0xcd04     /* there is another node with the same id */

#define LITESYNC_CMD_PING            0xcd05     /* check if alive */
#define LITESYNC_CMD_PONG            0xcd06     /* I am alive */

#define LITESYNC_LOG_INSERT          0xdb01     /* secondary -> primary */
#define LITESYNC_LOG_NEW             0xdb02     /* secondary <- primary -> secondary (broadcast) */
#define LITESYNC_LOG_NEW_OK          0xdb03     /* secondary -> primary */
#define LITESYNC_LOG_COMMIT          0xdb04     /* secondary <- primary -> secondary (broadcast) */

#define LITESYNC_LOG_NEXT            0xdb05     /* secondary -> primary (request) */
#define LITESYNC_LOG_GET             0xdb06     /* secondary -> primary (request) */
#define LITESYNC_LOG_NOTFOUND        0xdb07     /* secondary <- primary (response) */
#define LITESYNC_LOG_DATA            0xdb08     /* secondary <- primary (response) */
#define LITESYNC_IN_SYNC             0xdb09     /* secondary <- primary (response) */

#define LITESYNC_LOG_EXISTS          0xdb10     /* secondary <- primary (response) */
#define LITESYNC_LOG_INSERT_FAILED   0xdb11     /* secondary <- primary (response) */


/* peer message parameters */
#define LITESYNC_OK                  0xc0de001  /*  */
#define LITESYNC_ERROR               0xc0de002  /*  */

#define LITESYNC_NODE_ID             0xc0de004  /*  */

#define LITESYNC_SEQ                 0xc0de005  /*  */
#define LITESYNC_TID                 0xc0de006  /*  */
#define LITESYNC_SQL_CMDS            0xc0de007  /*  */
#define LITESYNC_PREV_TID            0xc0de008  /*  */
#define LITESYNC_HASH                0xc0de009  /*  */
//#define LITESYNC_LAST_TID            0xc0de010  /*  */


// the state of the slave peer or connection
#define STATE_CONN_NONE        0       /*  */
#define STATE_IDENTIFIED       1       /*  */
#define STATE_UPDATING         2       /*  */
#define STATE_IN_SYNC          3       /*  */
#define STATE_CONN_LOST        4       /*  */
#define STATE_INVALID_PEER     5       /*  */
#define STATE_BUSY             6       /*  */
#define STATE_ERROR            7       /*  */
#define STATE_RECEIVING_UPDATE 8       /* the slave is receiving a page change from the master while it is in the sync process */



struct tcp_address {
  struct tcp_address *next;
  char host[64];
  int port;
  int reconnect_interval;
};

struct connect_req {
  uv_connect_t connect;
  struct sockaddr_in addr;
};


#define LITESYNC_USE_SHORT_FILENAMES

#define DEFAULT_BACKLOG 128  /* used in uv_listen */

/* connection type */
#define CONN_UNKNOWN    0
#define CONN_INCOMING   1
#define CONN_OUTGOING   2

/* connection state */
#define CONN_STATE_UNKNOWN       0
#define CONN_STATE_CONNECTING    1
#define CONN_STATE_CONNECTED     2
#define CONN_STATE_CONN_LOST     3
#define CONN_STATE_DISCONNECTED  4
#define CONN_STATE_FAILED        5

/* db state - sync_down_state and sync_up_state */
#define DB_STATE_UNKNOWN         0  /* if there is no connection it doesn't know if there are remote changes */
#define DB_STATE_SYNCHRONIZING   1
#define DB_STATE_IN_SYNC         2
#define DB_STATE_LOCAL_CHANGES   3  /* there are local changes */
#define DB_STATE_OUTDATED        4  /* if the connection dropped while updating, or some other error state */


typedef struct litesync litesync;
typedef struct node node;


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

struct node_id_conflict {
  node *existing_node;
  node *new_node;
  uv_timer_t timer;
};

struct leader_votes {
  int id;
  int count;
  struct leader_votes *next;
};


struct node {
  node *next;            /* Next item in the list */
  int   id;              /* Node id */

  struct node_id_conflict *id_conflict;

  int   conn_type;       /* outgoing or incoming connection */
  char  host[64];        /* Remode IP address */
  int   port;            /* Remote port */

  uv_msg_t socket;       /* Socket used to connect with the other peer */
  int   conn_state;      /* The state of this connection/peer */

  litesync *this_node;   /* x */

  int num_txns;          /* How many transactions on its blockchain */

  /* used for the query status */
  int     db_state;
  uint64  last_conn;        /* (monotonic time) the last time a connection was made */
  uint64  last_conn_loss;   /* (monotonic time) the last time the connection was lost -- used??? */
  uint64  last_sync;        /* (monotonic time) the last time the db was synchronized with this node */
  uint64  time_out_of_date; /* (monotonic time) the first time a synchronization was not processed. cleared when the db is synchronized */
};


struct litesync {
  int id;                     /* Node id */

  char *uri;                  /* the URI filename with parameters */
  char *node_info;            /* User defined information about this node */

  struct tcp_address *bind;   /* Address(es) to bind */
  uv_udp_t *udp_sock;         /* Socket used for UDP communication */

  node *peers;                /* Remote nodes connected to this one */
  int total_known_nodes;      /* Including those that are currently off-line */

  BOOL is_leader;             /* True if this node is the current leader */
  node *leader_node;          /* Points to the leader node if it is connected */
  node *last_leader;          /* Points to the previous leader node */
  struct leader_votes *leader_votes;
  BOOL in_election;           /* True if in a leader election */

  struct transaction *mempool;

  single_instance_handle single_instance; /* store the handle for the single instance */

  sqlite3 *main_db1;          /* database connection for 'local' data - used by the app */
  sqlite3 *main_db2;          /* database connection for 'local' data - used by the worker thread */
  sqlite3 *worker_db;         /* database connection for 'remote' data */
  Pager *main_pager1;         /* pager for 'local' data - used by the app */
  Pager *main_pager2;         /* pager for 'local' data - used by the worker thread */
  Pager *worker_pager;        /* pager for 'remote' data */
  BOOL dbWasEmpty;            /* if the db was empty when open */
  char *extensions;           /* the SQLite extensions to be loaded */

  char worker_address[64];    /* unix socket or named pipe address for the worker thread */
  uv_thread_t worker_thread;  /* Thread used to receive dblog commands and events */
  int thread_running;         /* Whether the worker thread for this pager is running */
  int thread_active;          /* Whether the worker thread for this pager is active */
  uv_loop_t *loop;            /* libuv event loop for this thread */
  uv_timer_t log_rotation_timer;  /* Used to call the log rotation function again in the case of failure */
  uv_timer_t after_connections_timer;
  uv_timer_t process_transactions_timer;
  uv_timer_t failed_txn_timer;    /* Used to call the failed transaction function again in the case of failure */

  uv_timer_t leader_check_timer;
  uv_timer_t election_info_timer;
  uv_timer_t election_end_timer;

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
  int    num_blockchain_txns; /* number of transactions on the blockchain */

  BOOL   useSqliteRowids;     /* do not use node id in the rowids */

  BOOL   db_is_ready;         /* if the app can read and write from/to the database */
  u32    total_pages;         /* to calculate the download progress */
  u32    downloaded_pages;    /* to calculate the download progress */
  //int    db_state;
  int    sync_down_state;     /* (sync downstream state) UNKNOWN, SYNCING, IN_SYNC, OUTDATED */
  int    sync_up_state;       /* (sync upstream state)   HAS_LOCAL_CHANGES, SYNCING, IN_SYNC */
  uint64 local_changes_since; /* (monotonic time) the time since the db has local transaction not synchronized */
  uint64 last_sync;           /* (monotonic time) the last time the db was synchronized */
};


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
SQLITE_PRIVATE void litesyncStoreLogTransactionId(Pager *pPager);
SQLITE_PRIVATE void litesyncStoreLogTransactionTime(Pager *pPager);

SQLITE_PRIVATE sqlite_int64 litesyncBuildRowId(int node_id, u32 seq_num);
SQLITE_PRIVATE int  litesyncNodeIdFromRowId(sqlite_int64 value);
SQLITE_PRIVATE u32  litesyncSeqFromRowId(sqlite_int64 value);

SQLITE_PRIVATE int   litesyncGetWalLog(Pager *pPager, u32 start, int64 tid, binn **plog);
SQLITE_PRIVATE int64 last_tid_from_wal_log(Pager *pPager);

SQLITE_PRIVATE int  open_detached_worker_db(litesync *this_node, sqlite3 **pworker_db);
SQLITE_PRIVATE int  open_main_db_connection2(litesync *this_node);
SQLITE_PRIVATE int  open_worker_db(litesync *this_node);
SQLITE_PRIVATE void start_downstream_db_sync(litesync *this_node);
SQLITE_PRIVATE void start_upstream_db_sync(litesync *this_node);

SQLITE_PRIVATE void send_next_local_transaction(litesync *this_node);

SQLITE_PRIVATE int  send_notification_to_worker(char *address, void *data, int size);

SQLITE_PRIVATE void process_transactions_timer_cb(uv_timer_t* handle);
SQLITE_PRIVATE void reconnect_timer_cb(uv_timer_t* handle);
SQLITE_PRIVATE void log_rotation_timer_cb(uv_timer_t* handle);

SQLITE_PRIVATE void worker_thread_on_close(uv_handle_t *handle);


SQLITE_PRIVATE int is_local_ip_address(char *address);

SQLITE_PRIVATE int send_broadcast_message(litesync *this_node, char *message);
SQLITE_PRIVATE int send_udp_message(litesync *this_node, char *address, char *message);

SQLITE_PRIVATE void on_leader_check_timeout(uv_timer_t* handle);

SQLITE_PRIVATE void check_current_leader(litesync *this_node);
SQLITE_PRIVATE void start_leader_election(litesync *this_node);

SQLITE_PRIVATE void leader_node_process_local_transactions(litesync *this_node);

SQLITE_PRIVATE struct transaction * store_transaction_on_mempool(
  litesync *this_node, int node_id, int64 tid, void *log
);
SQLITE_PRIVATE void discard_mempool_transaction(litesync *this_node, struct transaction *txn);
SQLITE_PRIVATE int commit_transaction_to_blockchain(litesync *this_node, struct transaction *txn);

SQLITE_PRIVATE int check_if_failed_txn(litesync *this_node, int64 tid);

SQLITE_PRIVATE char * blockchain_status_json_db(sqlite3 *db, const char *name);
SQLITE_PRIVATE char * blockchain_status_json(Pager *pPager);
SQLITE_PRIVATE char * protocol_status_json_db(sqlite3 *db, const char *name, BOOL extended);
SQLITE_PRIVATE char * protocol_status_json(Pager *pPager, BOOL extended);

SQLITE_PRIVATE int set_current_node_info(sqlite3 *db, const char *name, char *info);
SQLITE_PRIVATE char * get_current_node_info(sqlite3 *db, const char *name);
