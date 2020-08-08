#include <stdbool.h>
#include "../common/uv_msg_framing.c"
#include "../common/uv_send_message.c"
#ifdef USE_UV_CALLBACK
#include "../common/uv_callback.c"
#endif
#include "../../core/sqlite3.h"
#define SQLITE_PRIVATE static
#include "../../common/sha256.h"
#include "secp256k1-vrf.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif


#define NEW_BLOCK_WAIT_INTERVAL  3000  /* default = 3 seconds */


/* peer communication */

#define PLUGIN_CMD                 0x43292173

#define PLUGIN_VERSION_NUMBER      1


/* message commands */

#define PLUGIN_VERSION             0xCD00

#define PLUGIN_CMD_ID              0xCD01     /* peer identification */
//#define PLUGIN_REQUEST_NODE_ID     0xCD02     /* request a node id */
//#define PLUGIN_NEW_NODE_ID         0xCD03     /* send the new node id */
#define PLUGIN_ID_CONFLICT         0xCD04     /* there is another node with the same id */

#define PLUGIN_GET_PEERS           0xCD11     /* request the list of peers */
#define PLUGIN_PEERS               0xCD12     /* list of peers */

#define PLUGIN_CMD_PING            0xCD21     /* check if alive */
#define PLUGIN_CMD_PONG            0xCD22     /* I am alive */

#define PLUGIN_TEXT                0xCD31     /* text message via TCP */


#define PLUGIN_REQUEST_STATE_DIFF  0xDB01
#define PLUGIN_UPTODATE            0xDB02
#define PLUGIN_DB_PAGE             0xDB03
#define PLUGIN_APPLY_UPDATE        0xDB04


#define PLUGIN_NEW_TRANSACTION     0x7001     /* follower <- leader -> follower (broadcast) */
#define PLUGIN_TRANSACTION_FAILED  0x7002     /* follower <- leader (response) */

#define PLUGIN_GET_MEMPOOL         0x7011     /* any -> any (request) */

#define PLUGIN_GET_TRANSACTION     0x7031     /* follower -> leader (request) */
#define PLUGIN_REQUESTED_TRANSACTION  0x7032     /* follower <- leader (response) */
#define PLUGIN_TXN_NOTFOUND        0x7033     /* follower <- leader (response) */


#define PLUGIN_NEW_BLOCK           0xB021     /* follower <- leader */
#define PLUGIN_BLOCK_VOTE          0xB022     /* follower -> all (broadcast) */

#define PLUGIN_GET_BLOCK           0xB041     /* follower -> leader (request) */
#define PLUGIN_REQUESTED_BLOCK     0xB042     /* follower <- leader (response) */
#define PLUGIN_BLOCK_NOTFOUND      0xB043     /* follower <- leader (response) */


#define PLUGIN_AUTHORIZATION       0xA001



/* message parameters */

#define PLUGIN_OK                  0xC0DE001  /*  */
#define PLUGIN_ERROR               0xC0DE002  /*  */

#define PLUGIN_PORT                0xC0DE101  /*  */
#define PLUGIN_NODE_ID             0xC0DE102  /*  */
#define PLUGIN_NODE_INFO           0xC0DE103  /*  */

#define PLUGIN_CPU                 0xC0DE201  /*  */
#define PLUGIN_OS                  0xC0DE202  /*  */
#define PLUGIN_HOSTNAME            0xC0DE203  /*  */
#define PLUGIN_APP                 0xC0DE204  /*  */

#define PLUGIN_RANDOM              0xC0DE301  /*  */

#define PLUGIN_PUBKEY              0xC0DE401  /*  */
#define PLUGIN_SIGNATURE           0xC0DE402  /*  */

#define PLUGIN_SEQ                 0xC0DE601  /*  */
#define PLUGIN_TID                 0xC0DE602  /*  */
#define PLUGIN_NONCE               0xC0DE603  /*  */
#define PLUGIN_SQL_CMDS            0xC0DE604  /*  */

#define PLUGIN_CONTENT             0xC0DE801
#define PLUGIN_PGNO                0xC0DE802
#define PLUGIN_DBPAGE              0xC0DE803
#define PLUGIN_STATE               0xC0DE804
#define PLUGIN_HEIGHT              0xC0DE805
#define PLUGIN_HEADER              0xC0DE806
#define PLUGIN_BODY                0xC0DE807
#define PLUGIN_MOD_PAGES           0xC0DE808
#define PLUGIN_HASH                0xC0DE809
#define PLUGIN_VOTES               0xC0DE80A

#define PLUGIN_PROOF               0xC0DE901




//#define BLOCK_HEIGHT    0x34
//#define STATE_HASH      0x35


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
  int is_broadcast;
  int reconnect_interval;
};

struct connect_req {
  uv_connect_t connect;
  struct sockaddr_in addr;
};


#define DEFAULT_BACKLOG 128  /* used in uv_listen */

/* connection type */
#define CONN_UNKNOWN    0
#define CONN_INCOMING   1
#define CONN_OUTGOING   2

/* connection type */

#define ADDRESS_BIND     1    /*  */
#define ADDRESS_CONNECT  2    /*  */

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
#define DB_STATE_ERROR           5


/* worker thread commands */
#define WORKER_THREAD_NEW_TRANSACTION  0xcd01  /*  */
#define WORKER_THREAD_EXIT             0xcd02  /*  */
#define WORKER_THREAD_OK               0xcd03  /*  */


typedef uint32_t Pgno;

typedef struct plugin plugin;
typedef struct node node;

struct node_id_conflict {
  node *existing_node;
  node *new_node;
  uv_timer_t timer;
};

struct node {
  node *next;            /* Next item in the list */
  int   id;              /* Node id */

  binn *conn_id;
  struct node_id_conflict *id_conflict;

  int   conn_type;       /* outgoing or incoming connection */
  char  host[64];        /* Remode IP address */
  int   port;            /* Remote port */
  int   bind_port;       /* Remote bind port. Used on incoming TCP connections */

  char  cpu[256];        /* CPU information */
  char  os[256];         /* OS information */
  char  hostname[256];   /* node's hostname */
  char  app[256];        /* application path and name */

  char  *info;           /* Dynamic information set by this peer */

  char  pubkey[36];      /* node's public key */
  int   pklen;           /* node's public key length */

  uv_msg_t socket;       /* Socket used to connect with the other peer */
  int   conn_state;      /* The state of this connection/peer */

  aergolite *this_node;  /* x */
  plugin *plugin;        /* x */

  int64 last_block;      /* The height of the last block */

  BOOL    is_authorized;
  BOOL    authorization_sent;
  BOOL    is_full_node;

  /* used for the query status */
  int     db_state;
};

struct node_nonce {
  int node_id;
  int64 last_nonce;
};

struct transaction {
  struct transaction *next;
  int node_id;
  int64 nonce;
  int64 id;
  char  datetime[24];
  void *log;
  int64 block_height;
};

#ifndef AERGOLITE_AMALGAMATION
struct block {
  struct block *next;
  int64 height;
  unsigned char id[32];
  void *header;
  void *body;
  void *votes;
  unsigned char vrf_proof[81];
  unsigned char vrf_output[32];
  unsigned int wait_time;
  int  num_votes;
  int  downloading_txns;
};
#endif

struct block_vote {
  struct block_vote *next;
  int   node_id;
  int64 height;
  uchar block_id[32];
  uchar sig[64];
};

struct txn_list {
  struct txn_list *next;
  void *log;
};

struct request {
  struct request *next;
  int64 transaction_id;
  void *contacted_nodes;
  uv_timer_t timer;
};


struct plugin {
  int node_id;                /* Node id */

  unsigned char *privkey;     /* the private key for this node */
  char *pubkey;               /* the public key for this node */
  int pklen;                  /* public key length */
  secp256k1_pubkey *pubkey_obj; /* the public key object */

  aergolite *this_node;       /* Reference to the aergolite instance */

  struct tcp_address *bind;   /* Address(es) to bind */
  struct tcp_address *discovery;  /* Node discovery address(es) */
  struct tcp_address *broadcast;  /* Broadcast address(es) */
  uv_udp_t *udp_sock;         /* Socket used for UDP communication */

  node *peers;                /* Remote nodes connected to this one */
  int total_authorized_nodes; /* Including those that are currently off-line */

  BOOL is_authorized;         /* Whether this node is authorized on the network */
  BOOL is_full_node;

  struct request *requests;

  struct transaction *mempool;
  struct txn_list *special_txn; /* New special transaction */

  void *nonces;

  struct block *current_block;
  struct block *new_blocks;
  struct block *open_block;
  struct block_vote *block_votes;
  int64  last_created_block_height;
  int64  last_vote_height;

  int block_interval;

  unsigned char block_vrf_proof[81];
  unsigned char block_vrf_output[32];

#ifdef USE_UV_CALLBACK
  uv_callback_t worker_cb;    /* callback handle to send msg to the worker thread */
#else
  char worker_address[64];    /* unix socket or named pipe address for the worker thread */
#endif
  uv_thread_t worker_thread;  /* Thread used to receive dblog commands and events */
  int thread_running;         /* Whether the worker thread for this pager is running */
  int thread_active;          /* Whether the worker thread for this pager is active */
  uv_loop_t *loop;            /* libuv event loop for this thread */

  uv_timer_t aergolite_core_timer;  /* Timer for the aergolite periodic function */

  uv_timer_t after_connections_timer;

  uv_timer_t state_update_timer;

  uv_timer_t process_transactions_timer;

  uv_timer_t new_block_timer;
  uv_timer_t block_wait_timer;

  uv_timer_t reconnect_timer;
  int reconnect_timer_enabled;

  sqlite3_mutex *mutex;

  bool is_updating_state;
  int  contacted_node_id;
  void *state_update_contacted_nodes;
  void *state_update_failed_nodes;

  int sync_down_state;        /* downstream synchronization state */
  int sync_up_state;          /* upstream synchronization state */
};


/* peers and network */

SQLITE_PRIVATE int is_local_ip_address(char *address);

SQLITE_PRIVATE int send_tcp_broadcast(plugin *plugin, char *message);
SQLITE_PRIVATE int send_udp_broadcast(plugin *plugin, char *message);
SQLITE_PRIVATE int send_udp_message(plugin *plugin, const struct sockaddr *address, char *message);

SQLITE_PRIVATE void on_text_command_received(node *node, char *message);

SQLITE_PRIVATE BOOL has_nodes_for_consensus(plugin *plugin);

/* general */

SQLITE_PRIVATE int random_number(int lower, int upper);

/* mempool */

SQLITE_PRIVATE int  store_transaction_on_mempool(
  plugin *plugin, int node_id, int64 nonce, void *log, struct transaction **ptxn
);
SQLITE_PRIVATE void discard_mempool_transaction(plugin *plugin, struct transaction *txn);
SQLITE_PRIVATE int  check_mempool_transactions(plugin *plugin);

/* transactions */

SQLITE_PRIVATE void on_new_remote_transaction(node *node, void *msg, int size);
SQLITE_PRIVATE bool process_arrived_transaction(plugin *plugin, struct transaction *txn);

/* blockchain */

SQLITE_PRIVATE void start_downstream_db_sync(plugin *plugin);
SQLITE_PRIVATE void start_upstream_db_sync(plugin *plugin);

SQLITE_PRIVATE int  load_current_state(plugin *plugin);
SQLITE_PRIVATE void request_state_update(plugin *plugin);

/* blocks */

SQLITE_PRIVATE void start_new_block_timer(plugin *plugin);
SQLITE_PRIVATE int  broadcast_new_block(plugin *plugin, struct block *block);
SQLITE_PRIVATE void send_new_block(plugin *plugin, node *node, struct block *block);
SQLITE_PRIVATE void send_new_blocks(plugin *plugin, node *node);
SQLITE_PRIVATE void send_block_votes(plugin *plugin, node *node);
SQLITE_PRIVATE void rollback_block(plugin *plugin);
SQLITE_PRIVATE void discard_uncommitted_blocks(plugin *plugin);

/* event loop and timers */

SQLITE_PRIVATE void process_transactions_timer_cb(uv_timer_t* handle);
SQLITE_PRIVATE void reconnect_timer_cb(uv_timer_t* handle);
SQLITE_PRIVATE void log_rotation_timer_cb(uv_timer_t* handle);

SQLITE_PRIVATE void enable_reconnect_timer(plugin *plugin);

SQLITE_PRIVATE void worker_thread_on_close(uv_handle_t *handle);

SQLITE_PRIVATE int  send_notification_to_worker(plugin *plugin, void *data, int size);

/* UDP messages */

typedef void (*udp_message_callback)(plugin *plugin, uv_udp_t *socket, const struct sockaddr *addr, char *sender, char *arg);

struct udp_message {
  char name[32];
  udp_message_callback callback;
};

SQLITE_PRIVATE void register_udp_message(char *name, udp_message_callback callback);

/* TCP text messages */

typedef void (*tcp_message_callback)(plugin *plugin, node *node, char *arg);

struct tcp_message {
  char name[32];
  tcp_message_callback callback;
};

SQLITE_PRIVATE void register_tcp_message(char *name, tcp_message_callback callback);

/* node discovery */

SQLITE_PRIVATE void start_node_discovery(plugin *plugin);

SQLITE_PRIVATE void request_peer_list(plugin *plugin, node *node);
SQLITE_PRIVATE void send_peer_list(plugin *plugin, node *to_node);
SQLITE_PRIVATE void on_peer_list_request(node *node, void *msg, int size);
SQLITE_PRIVATE void on_peer_list_received(node *node, void *msg, int size);

/* node authorization */

SQLITE_PRIVATE int  on_new_authorization(plugin *plugin, void *log, BOOL from_network);
SQLITE_PRIVATE void count_authorized_nodes(plugin *plugin);

SQLITE_PRIVATE void plugin_update_nodes_types(plugin *plugin);
