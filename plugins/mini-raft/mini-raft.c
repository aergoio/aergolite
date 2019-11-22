#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <ctype.h>   /* for isdigit */
#include <unistd.h>  /* for unlink */

#include "mini-raft.h"

#include "../../common/array.c"
#include "../../common/linked_list.c"

#include "../common/uv_functions.c"

#define majority(X)  X / 2 + 1

/*****************************************************************************/

SQLITE_PRIVATE char * getSyncState(int state) {

  switch(state){
  case DB_STATE_SYNCHRONIZING:
    return "synchronizing";
  case DB_STATE_IN_SYNC:
    return "in sync";
  case DB_STATE_LOCAL_CHANGES:
    return "with local changes";
  case DB_STATE_OUTDATED:
    return "outdated";
  default:
    return "unknown";
  }

}

/*****************************************************************************/

SQLITE_PRIVATE char * getConnState(int state) {

  switch( state ){
  case CONN_STATE_CONNECTING:
    return "connecting";
  case CONN_STATE_CONNECTED:
    return "connected";
  case CONN_STATE_CONN_LOST:
    return "connection lost";
  case CONN_STATE_DISCONNECTED:
    return "disconnected";
  case CONN_STATE_FAILED:
    return "failed";
  default:
    return "unknown";
  }

}

/*****************************************************************************/

SQLITE_PRIVATE char * getConnType(int type) {

  switch( type ){
  case CONN_INCOMING:
    return "incoming";
  case CONN_OUTGOING:
    return "outgoing";
  default:
    return "unknown";
  }

}

/*****************************************************************************/

SQLITE_API char * get_protocol_status(void *arg, BOOL extended) {
  plugin *plugin = (struct plugin *) arg;
  aergolite *this_node = plugin->this_node;
  struct node *node;
  char buf[64];
  sqlite3_str *str = sqlite3_str_new(NULL);

  sqlite3_str_appendall(str, "{\n\"use_blockchain\": true,\n");

  sqlite3_str_appendf(str, "\"node_id\": %d,\n", plugin->node_id);
  sqlite3_str_appendf(str, "\"is_leader\": %s,\n", plugin->is_leader ? "true" : "false");
//sqlite3_str_appendf(str, "\"db_is_ready\": %s,\n", plugin->db_is_ready ? "true" : "false");

  if( extended ){

    sqlite3_str_appendf(str, "\"peers\": [");

    for( node=plugin->peers; node; node=node->next ){
      if( node->conn_state!=CONN_STATE_CONNECTED ) continue;
      if( !node->is_authorized ) continue;

      if( node != plugin->peers ){
        sqlite3_str_appendchar(str,1, ',');
      }
      sqlite3_str_appendall(str, "{\n");

      sqlite3_str_appendf(str, "  \"node_id\": %d,\n", node->id);
      if( node->info ){
        sqlite3_str_appendf(str, "  \"node_info\": \"%s\",\n", node->info);
      }
      sqlite3_str_appendf(str, "  \"is_leader\": %s,\n", (node==plugin->leader_node) ? "true" : "false");
      sqlite3_str_appendf(str, "  \"conn_type\": \"%s\",\n", getConnType(node->conn_type));
      sqlite3_str_appendf(str, "  \"address\": \"%s:%d\"\n", node->host, node->port);
      //sqlite3_str_appendf(str, "  \"conn_state\": \"%s\",\n", getConnState(node->conn_state));
      //sqlite3_str_appendf(str, "  \"db_state\": \"%s\"\n", getSyncState(node->db_state));
      //sqlite3_str_appendf(str, "  \"last_sync\": %lld\n", node->last_sync);

      sqlite3_str_appendchar(str, 1, '}');
    }

    sqlite3_str_appendall(str, "],\n");

  }else{

    int num_peers;

    if( plugin->leader_node ){
      sqlite3_snprintf(64, buf, "%d", plugin->leader_node->id);
    }else{
      strcpy(buf, "null");
    }

    num_peers = 0;
    for( node=plugin->peers; node; node=node->next ){
      if( node->conn_state==CONN_STATE_CONNECTED ){
        num_peers++;
      }
    }

    sqlite3_str_appendf(str, "\"leader\": %s,\n", buf);
    sqlite3_str_appendf(str, "\"num_peers\": %d,\n", num_peers);

  }

  sqlite3_str_appendall(str, "\"mempool\": {\n");

  {
    struct transaction *txn, *last = NULL;
    int num_transactions = 0;

    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->block_height==0 ){
        num_transactions++;
        last = txn;
      }
    }

    sqlite3_str_appendf(str, "  \"num_transactions\": %d", num_transactions);

    if( last ){
      sqlite3_str_appendall(str, ",\n  \"last_transaction\": ");
      if( extended ){
        char *timestamp=NULL;
        binn_list_get(last->log, binn_count(last->log), BINN_DATETIME, &timestamp, NULL);
        sqlite3_str_appendall(str, "{\n");
        sqlite3_str_appendf(str, "    \"id\": %lld,\n", last->id);
        sqlite3_str_appendf(str, "    \"node_id\": %d,\n", last->node_id);
        sqlite3_str_appendf(str, "    \"timestamp\": \"%s\"\n", timestamp);
        sqlite3_str_appendall(str, "  }");
      }else{
        sqlite3_str_appendf(str, "%lld", last->id);
      }
    }

    sqlite3_str_appendchar(str, 1, '\n');
  }

  sqlite3_str_appendall(str, "},\n");

  sqlite3_str_appendf(str, "\"sync_down_state\": \"%s\",\n", getSyncState(plugin->sync_down_state));

#if 0
  if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING ){
    double progress = 0.0;
    if( plugin->total_pages>0 ){  /* to avoid division by zero */
      progress = ((double)plugin->downloaded_pages) / plugin->total_pages * 100;
    }
    sqlite3_str_appendf(str, "\"sync_down_progress\": %f,\n", progress);
  }
#endif

  sqlite3_str_appendf(str, "\"sync_up_state\": \"%s\"\n", getSyncState(plugin->sync_up_state));

  //sqlite3_str_appendf(str, "\"last_sync\": %lld\n", plugin->last_sync);

  sqlite3_str_appendchar(str, 1, '}');

  return sqlite3_str_finish(str);
}

/****************************************************************************/

struct print_node {
  plugin *plugin;
  void   *vdbe;
};

SQLITE_PRIVATE void print_allowed_node_cb(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  void *authorization,
  int64 last_nonce
){
  struct print_node *data = (struct print_node *) arg;
  struct node *node;
  char hexpubkey[72];

  /* ignore if it is this node */
  if( memcmp(data->plugin->pubkey,pubkey,pklen)==0 ) return;

  /* ignore if it is from a connected node */
  for(node=data->plugin->peers; node; node=node->next){
    if( node->conn_state==CONN_STATE_CONNECTED &&
        memcmp(node->pubkey,pubkey,pklen)==0 ) return;
  }

  /* print the offline node */
  to_hex(pubkey, pklen, hexpubkey);
  node_list_add(data->vdbe, node_id, hexpubkey, "", "", "", "", "", "", "");

}

/****************************************************************************/

// node_id | pubkey | address | CPU | OS | hostname | app | extra_info | external |

SQLITE_API void print_node_list(void *arg, void *vdbe) {
  plugin *plugin = (struct plugin *) arg;
  aergolite *this_node = plugin->this_node;
  struct node *node;
  int64 last_block = plugin->current_block ? plugin->current_block->height : 0;
  char hostname[256], cpu[256], os[256], app[256], *node_info;
  char *pubkey, hexpubkey[72], address[32];
  int pklen;

  /* add this node to the list of nodes (always the first one) */

  pubkey = aergolite_pubkey(this_node, &pklen);
  if( pubkey )
    to_hex(pubkey, pklen, hexpubkey);
  else
    hexpubkey[0] = 0;
  sprintf(address, "%s:%d", plugin->bind->host, plugin->bind->port);
  get_this_device_info(hostname, cpu, os, app);
  node_info = aergolite_get_node_info(this_node);

  node_list_add(vdbe,
     plugin->node_id,
     hexpubkey,
     address,
     cpu,
     os,
     hostname,
     app,
     node_info ? node_info : "",
     last_block==0 ? "yes" : "");

  /* iterate over the connected nodes */

  for(node=plugin->peers; node; node=node->next){
    if( node->conn_state!=CONN_STATE_CONNECTED ) continue;

    to_hex(node->pubkey, node->pklen, hexpubkey);
    sprintf(address, "%s:%d", node->host, node->port);

    node_list_add(vdbe,
       node->id,
       hexpubkey,
       address,
       node->cpu,
       node->os,
       node->hostname,
       node->app,
       node->info ? node->info : "",
       node->is_authorized ? "" : "yes");

  }

  /* iterate over the allowed nodes table */

  aergolite_iterate_allowed_nodes(this_node, print_allowed_node_cb,
    &(struct print_node){ plugin, vdbe });

}

/****************************************************************************/

/* a broadcast message requesting blockchain status info */
SQLITE_PRIVATE void on_blockchain_status_request(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *sender,
  char *sender_ip,
  char *arg
){
  aergolite *this_node = plugin->this_node;

  char *status = aergolite_get_blockchain_status(this_node);
  if( status ){
    send_udp_message(plugin, sender, status);
    sqlite3_free(status);
  }

}

/****************************************************************************/

/* a broadcast message requesting protocol status info */
SQLITE_PRIVATE void on_protocol_status_request(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *sender,
  char *sender_ip,
  char *arg
){
  char *status;
  BOOL extended = 0;

  if( arg && strcmp(arg,"1")==0 ){  /* extended status info */
    extended = 1;
  }

  status = get_protocol_status(plugin, extended);
  if( status ){
    send_udp_message(plugin, sender, status);
    sqlite3_free(status);
  }

}

/****************************************************************************/

/* a broadcast message requesting node info */
SQLITE_PRIVATE void on_node_info_request(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *sender,
  char *sender_ip,
  char *arg
){
  aergolite *this_node = plugin->this_node;

  char *info = aergolite_get_node_info(this_node);

  if( info ){
    send_udp_message(plugin, sender, info);
  }else{
    send_udp_message(plugin, sender, "");
  }

}

/*****************************************************************************/

/*
** This app changed the node info.
** Sends the updated information to all the connected peers.
*/
SQLITE_API void on_local_node_info_changed(void *arg, char *node_info) {
  plugin *plugin = (struct plugin *) arg;
  char *msg;

  msg = sqlite3_mprintf("node_info:%s", node_info);
  if( !msg ) return;

  send_tcp_broadcast(plugin, msg);

  sqlite3_free(msg);

}

/****************************************************************************/

/* node info */
SQLITE_PRIVATE void on_peer_info_changed(
  plugin *plugin,
  node *node,
  char *arg
){

  sqlite3_free(node->info);

  if( arg ){
    node->info = sqlite3_strdup(arg);
  }else{
    node->info = NULL;
  }

}

/***************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void reset_node_state(plugin *plugin){

  if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING || plugin->sync_down_state==DB_STATE_IN_SYNC ){
    plugin->sync_down_state = DB_STATE_UNKNOWN;
  }

  //if( plugin->sync_up_state==DB_STATE_SYNCHRONIZING || plugin->sync_up_state==DB_STATE_IN_SYNC ){
  //  plugin->sync_up_state = DB_STATE_UNKNOWN;
  //}
  if( plugin->sync_up_state==DB_STATE_SYNCHRONIZING ){
    plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void discard_block(struct block *block) {

  SYNCTRACE("discard_block\n");

  if( !block ) return;

  if( block->header     ) sqlite3_free(block->header);
  if( block->body       ) sqlite3_free(block->body);
  if( block->signatures ) sqlite3_free(block->signatures);
  sqlite3_free(block);

}

/****************************************************************************/

#include "state_update.c"
#include "transactions.c"
#include "consensus.c"

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void start_downstream_db_sync(plugin *plugin) {
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("start_downstream_synchronization\n");

  if( !plugin->current_block ){
    int rc = load_current_state(plugin);
    if( rc==SQLITE_BUSY || rc==SQLITE_NOMEM ){
      /* do not request download. it was a failure on the state loading. try again later */
      plugin->sync_down_state = DB_STATE_ERROR;
      return;
    }else if( rc!=SQLITE_EMPTY && rc!=SQLITE_INVALID ){
      /* do not request download of the db from other node on unexpected error */
      plugin->sync_down_state = DB_STATE_ERROR;
      return;
    }
  }

  plugin->sync_down_state = DB_STATE_SYNCHRONIZING;

  request_state_update(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void start_upstream_db_sync(plugin *plugin) {

  SYNCTRACE("start_upstream_db_sync\n");

  /* send the local transactions */
  send_local_transactions(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void check_base_db(plugin *plugin) {
  //aergolite *this_node = plugin->this_node;

  SYNCTRACE("check_base_db\n");

// it could check if the other nodes have the same blockchain / database

  /* start the db synchronization */
//  start_downstream_db_sync(plugin);

//  check_current_leader(plugin);

}

/****************************************************************************/

#ifdef _WIN32
#define PERIODIC_INTERVAL 1000
#else
#define PERIODIC_INTERVAL 1000
#endif

SQLITE_PRIVATE void aergolite_core_timer_cb(uv_timer_t* handle){
  plugin *plugin = (struct plugin *) handle->loop->data;

  if( !plugin->is_updating_state ){
    aergolite_periodic(plugin->this_node);
  }

  check_current_leader(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void after_connections_timer_cb(uv_timer_t* handle){
  plugin *plugin = (struct plugin *) handle->loop->data;

  check_base_db(plugin);

  //! this could be in another place if not doing anything with the base db
  uv_timer_start(&plugin->aergolite_core_timer, aergolite_core_timer_cb, PERIODIC_INTERVAL, PERIODIC_INTERVAL);

}

/****************************************************************************/
/****************************************************************************/

/*
** On follower nodes this function exists for continuing the synchronization
** process when it is stopped due to a failure.
**
** Maybe it could also work when it sends a request to the leader node but
** no answer is returned.
*/
SQLITE_PRIVATE void process_transactions_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;
  if( plugin->is_leader ){
    leader_node_process_local_transactions(plugin);
  }else{
    //follower_node_process_local_transactions(plugin);
    if( !plugin->leader_node ) return;
    /* downstream synchronization */
    if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
      start_downstream_db_sync(plugin);
    }
    /* upstream synchronization */
    if( plugin->sync_down_state==DB_STATE_IN_SYNC ){
      if( plugin->sync_up_state!=DB_STATE_SYNCHRONIZING && plugin->sync_up_state!=DB_STATE_IN_SYNC ){
        send_local_transactions(plugin);
      }
    }
  }
}

/****************************************************************************/
/****************************************************************************/

#include "allowed_nodes.c"

SQLITE_PRIVATE node * node_from_socket(uv_msg_t *socket) {
  uv_loop_t *loop = ((uv_handle_t*)socket)->loop;
  plugin *plugin = (struct plugin *) loop->data;
  aergolite *this_node = plugin->this_node;
  node *node;

  SYNCTRACE("node_from_socket - this_node=%p socket=%p\n", this_node, socket);

  if (!this_node) return NULL;

  for(node=plugin->peers; node; node=node->next){
    if( &node->socket==socket ) return node;
  }

  SYNCTRACE("node_from_socket - NOT FOUND\n");
  return NULL;
}

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_connected(node *node) {
  plugin *plugin = node->plugin;

  if( !node ) return;

  SYNCTRACE("new node connected: %s:%d ---\n", node->host, node->port);

  // it could call a user callback function to ... identify/authenticate it, or simply to log

  if( send_node_identification(plugin, node)==FALSE ){
    disconnect_peer(node);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_node_disconnected(node *node) {
  plugin *plugin;
  aergolite *this_node;

  if (node == NULL) return;
  plugin = node->plugin;
  this_node = node->this_node;

  SYNCTRACE("--- node disconnected: %s:%d leader=%s\n", node->host, node->port,
            node==plugin->leader_node?"yes":"no");

  if( node==plugin->last_leader ){
    plugin->last_leader = NULL;
  }

  if( node==plugin->leader_node ){
    plugin->leader_node = NULL;
    reset_node_state(plugin);
    uv_timer_stop(&plugin->process_transactions_timer);
    if( plugin->thread_active ){
      check_current_leader(plugin);
      //start_leader_election(plugin);
    }
  }

  //enable_reconnect_timer(plugin);

  // it can log, notify the app, reconnect

}

/****************************************************************************/

SQLITE_PRIVATE node * new_node(uv_loop_t *loop) {
  plugin *plugin = (struct plugin *) loop->data;
  aergolite *this_node = plugin->this_node;
  node *node;
  int rc;

  /* allocate a new node/peer struct */
  node = sqlite3_malloc_zero(sizeof(struct node));
  if (!node) { sqlite3_log(SQLITE_ERROR, "worker thread: could not allocate new node"); return NULL; }

  /* initialize the socket */
  if ((rc = uv_msg_init(loop, &node->socket, UV_TCP))) {
    sqlite3_log(SQLITE_ERROR, "worker thread: could not initialize TCP socket: (%d) %s", rc, uv_strerror(rc));
    sqlite3_free(node->info);
    sqlite3_free(node);
    return NULL;
  }

  /* add this node to the list of peers for this db */
  node->plugin = plugin;
  node->this_node = this_node;
  llist_add(&plugin->peers, node);

  return node;

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_peer_message(uv_msg_t *stream, void *msg, int size);

/* process either incoming or outgoing connections */
SQLITE_PRIVATE int worker_thread_on_tcp_connection(node *node) {
  int rc;

  /* start reading messages on the connection */
  rc = uv_msg_read_start(&node->socket, alloc_buffer, worker_thread_on_peer_message, free_buffer);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not start read: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }

  rc = uv_tcp_nodelay( (uv_tcp_t*) &node->socket, 1);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not set TCP no delay: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }

  rc = uv_tcp_keepalive( (uv_tcp_t*) &node->socket, 1, 180);  /* 180 seconds = 3 minutes */
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not set TCP keep alive: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }

  /* set the state as connected */
  node->conn_state = CONN_STATE_CONNECTED;

  return SQLITE_OK;

loc_failed:
  uv_close2((uv_handle_t*) &node->socket, worker_thread_on_close);
  return rc;

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_outgoing_tcp_connection(uv_connect_t *connect, int status) {
  uv_msg_t *socket = (uv_msg_t *) connect->handle;
  node *node = node_from_socket(socket);
  int rc;

  sqlite3_free(connect);

  if (status < 0) {
    sqlite3_log(SQLITE_ERROR, "worker_thread_on_outgoing_tcp_connection failed: (%d) %s", status, uv_strerror(status));
loc_failed:
    /* closing the socket will start the reconnection timer */
    uv_close2((uv_handle_t *) socket, worker_thread_on_close);
    return;
  }

  if (!node) {
    SYNCTRACE("worker_thread_on_outgoing_tcp_connection: node=NULL\n");
    return;  //goto loc_failed;
  }

  /* prepare the connection */
  rc = worker_thread_on_tcp_connection(node);
  if (rc < 0) goto loc_failed;

  on_new_node_connected(node);

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_incoming_tcp_connection(uv_stream_t *server, int status) {
//  aergolite *this_node;
  node *node;
  int rc;

  if (status < 0) {
    sqlite3_log(SQLITE_ERROR, "worker_thread_on_incoming_tcp_connection failed: (%d) %s", status, uv_strerror(status));
    return;
  }

  SYNCTRACE("worker_thread_on_incoming_tcp_connection - new connection\n");

  /* allocate a new node/peer struct */
  node = new_node(server->loop);
  if (!node) { sqlite3_log(SQLITE_ERROR, "worker thread: could not allocate new node"); return; }

//  this_node = (aergolite *) server->data;

  /* accept the connection */
  if ((rc = uv_accept(server, (uv_stream_t*) &node->socket)) == 0) {
    struct sockaddr_storage peeraddr;
    int namelen = sizeof(peeraddr);

    if (uv_tcp_getpeername((uv_tcp_t *) &node->socket, (struct sockaddr *) &peeraddr, &namelen) == 0) {
      if( peeraddr.ss_family == AF_INET6 ){
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &peeraddr;
        uv_ip6_name(addr, node->host, sizeof(node->host));
        node->port = ntohs(addr->sin6_port);
      } else {
        struct sockaddr_in *addr = (struct sockaddr_in *) &peeraddr;
        uv_ip4_name(addr, node->host, sizeof(node->host));
        node->port = ntohs(addr->sin_port);
      }
    }

    SYNCTRACE("new connection from %s:%d\n", node->host, node->port);

    node->conn_type = CONN_INCOMING;

    /* prepare the connection */
    rc = worker_thread_on_tcp_connection(node);
    if (rc < 0) goto loc_failed;

    on_new_node_connected(node);

  } else {
    sqlite3_log(SQLITE_ERROR, "worker thread: could not accept TCP connection: (%d) %s", rc, uv_strerror(rc));
loc_failed:
    uv_close2((uv_handle_t*) &node->socket, worker_thread_on_close);
  }

}

/****************************************************************************/

SQLITE_PRIVATE int connect_to_peer_address(node *node, struct sockaddr *dest) {
  uv_connect_t* connect;
  int rc;

  connect = sqlite3_malloc(sizeof(uv_connect_t));
  if (!connect) {
    sqlite3_log(SQLITE_NOMEM, "worker thread: no memory");
    node->conn_state = CONN_STATE_FAILED;
    return SQLITE_NOMEM;
  }

  if ((rc = uv_tcp_connect(connect, (uv_tcp_t*) &node->socket, dest, worker_thread_on_outgoing_tcp_connection))) {
    sqlite3_log(SQLITE_ERROR, "worker thread: connect to TCP address [%s:%d] failed: (%d) %s", node->host, node->port, rc, uv_strerror(rc));
    node->conn_state = CONN_STATE_FAILED;
    uv_close2((uv_handle_t*) &node->socket, worker_thread_on_close);
    return SQLITE_ERROR;
  }

  return SQLITE_OK;

}

/****************************************************************************/

SQLITE_PRIVATE void on_resolved(uv_getaddrinfo_t *resolver, int status, struct addrinfo *res) {
  node *node;
#ifdef DEBUGSYNC
  char buf[32];
#endif

  if (status < 0) {
    sqlite3_log(SQLITE_ERROR, "getaddrinfo callback error: %s\n", uv_err_name(status));
    goto loc_exit;
  }

  node = (struct node*) resolver->data;

#ifdef DEBUGSYNC
  if( res->ai_addr->sa_family == AF_INET6 ){
    uv_ip6_name((struct sockaddr_in6*) res->ai_addr, buf, 32);
  } else {
    uv_ip4_name((struct sockaddr_in*) res->ai_addr, buf, 32);
  }
  SYNCTRACE("address resolved to %s\n", buf);
#endif

  connect_to_peer_address(node, res->ai_addr);

  uv_freeaddrinfo(res);
loc_exit:
  sqlite3_free(resolver);

}

/****************************************************************************/

SQLITE_PRIVATE int connect_to_peer(node *node) {
  struct sockaddr_storage dest;
  int rc;

  if( !node ){
    sqlite3_log(SQLITE_ERROR, "connect_to_peer: node==null");
    return SQLITE_ERROR;
  }

  if( ((uv_handle_t*)&node->socket)->loop == 0 ){
    /* initialize the socket */
    SYNCTRACE("connect_to_peer - initializing socket\n");
    if ((rc = uv_msg_init(node->plugin->loop, &node->socket, UV_TCP))) {
      sqlite3_log(SQLITE_ERROR, "worker thread: could not initialize TCP socket: (%d) %s", rc, uv_strerror(rc));
      return SQLITE_ERROR;
    }
  }

  node->bind_port = node->port;
  node->conn_state = CONN_STATE_CONNECTING;

  if( uv_ip4_addr(node->host, node->port, (struct sockaddr_in *) &dest)==0 ){
    return connect_to_peer_address(node, (struct sockaddr *) &dest);
  } else if( uv_ip6_addr(node->host, node->port, (struct sockaddr_in6 *) &dest)==0 ){
    return connect_to_peer_address(node, (struct sockaddr *) &dest);
  } else {  /* try to resolve the address */
    uv_getaddrinfo_t *resolver;
    struct addrinfo hints;
    char port[8];
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    SYNCTRACE("resolving address %s ...\n", node->host);

    resolver = sqlite3_malloc(sizeof(uv_getaddrinfo_t));
    if( !resolver ) return SQLITE_ERROR;
    resolver->data = node;

    sprintf(port, "%d", node->port);  /* itoa does not exist on Linux */
    rc = uv_getaddrinfo(node->plugin->loop, resolver, on_resolved, node->host, port, &hints);
    if( rc ) return SQLITE_ERROR;
  }

  return SQLITE_OK;
}

/****************************************************************************/
/*
SQLITE_PRIVATE void reconnect_timer_cb(uv_timer_t* handle) {
  struct node *node = (struct node *) handle->data;

  SYNCTRACE("reconnect_timer_cb\n");

  connect_to_peer(node);

  // it must stop the timer:
  // -on successful connection
  // -when the event loop is closing

}
*/

/*
** The connection to the leader node dropped.
** This node can be off-line. This is detected when this node executes
** a new transaction and tries it to send it to the leader node.
*/
SQLITE_PRIVATE void reconnect_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("reconnect_timer_cb\n");

  /*
  ** Send periodic broadcast messages to the peers.
  ** This will also enable the after connections timer on the first response.
  */

  start_node_discovery(plugin);

  // it must stop the timer:
  // -on successful connection
  // -when the event loop is closing

}

/****************************************************************************/

/*
** Do not call this function with the remote connected port in case of incoming
** TCP connections. Use the remote bind port.
*/
SQLITE_PRIVATE void check_peer_connection(plugin *plugin, char *ip_address, int port) {
  node *node;

  SYNCTRACE("check_peer_connection %s:%d\n", ip_address, port);

  if( is_local_ip_address(ip_address) && port==plugin->bind->port ) return;

  /* check if already connected to this peer */

  for( node=plugin->peers; node; node=node->next ){
    if( strcmp(node->host,ip_address)==0 && node->bind_port==port ){
      if( node->conn_state==CONN_STATE_CONNECTING || node->conn_state==CONN_STATE_CONNECTED ){
        SYNCTRACE("check_peer_connection: %s:%d already connected\n", ip_address, port);
        return;
      }else{
        SYNCTRACE("check_peer_connection: reconnecting to %s:%d\n", ip_address, port);
        goto loc_reconnect;
      }
    }
  }

  /* connect to this peer */

  SYNCTRACE("check_peer_connection: connecting to %s:%d\n", ip_address, port);

  node = new_node(plugin->loop);
  if( !node ) return;

  strcpy(node->host, ip_address);
  node->port = port;

loc_reconnect:

  node->conn_type = CONN_OUTGOING;
  connect_to_peer(node);

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void worker_thread_cmd_exit(uv_loop_t *loop) {

  SYNCTRACE("worker_thread_cmd_exit\n");

  uv_stop(loop);

}

/****************************************************************************/
/*
SQLITE_PRIVATE void enable_node_reconnect_timer(struct node *node) {

  if( !node->reconnect_timer_enabled ){
    uv_timer_start(&node->reconnect_timer, reconnect_timer_cb,
                    reconnect_interval,
                    reconnect_interval);
    node->reconnect_timer_enabled = 1;
  }

}
*/

#define leader_reconnect_interval 2000

SQLITE_PRIVATE void enable_reconnect_timer(plugin *plugin) {

  if( !plugin->reconnect_timer_enabled ){
    uv_timer_start(&plugin->reconnect_timer, reconnect_timer_cb,
                   leader_reconnect_interval,
                   leader_reconnect_interval);
    plugin->reconnect_timer_enabled = 1;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_close(uv_handle_t *handle) {

  SYNCTRACE("worker_thread_on_(handle)_close - handle=%p\n", handle);

  switch( handle->type ){
  case UV_NAMED_PIPE:
    SYNCTRACE("worker_thread_on_(handle)_close - named pipe\n");
    sqlite3_free(handle);
    break;
  case UV_TCP: {
    //uv_loop_t *loop = handle->loop;
    node *node = node_from_socket((uv_msg_t*)handle);   //! what if this fn fails... if the node was already removed...
    SYNCTRACE("worker_thread_on_(handle)_close - TCP\n");
    if( node ){  /* this is a client socket */
      plugin *plugin = (struct plugin *) handle->loop->data;
      aergolite *this_node = plugin->this_node;
      if( node->conn_state==CONN_STATE_CONNECTED ){
        /* only fires the event if the node was connected */
        on_node_disconnected(node);
      }
      SYNCTRACE("socket closed - releasing node\n");
      llist_remove(&plugin->peers, node);
      if( node->id_conflict ){
        stop_id_conflict_timer(node->id_conflict);
      }
      sqlite3_free(node->info);
      sqlite3_free(node);

      //enable_node_reconnect_timer(node); //! it can activate a timer when there is no more peers

    } else {  /* this is the listening socket */
      sqlite3_free(handle);
    }
    break;
   }
  case UV_UDP:
    SYNCTRACE("worker_thread_on_(handle)_close - UDP\n");
    sqlite3_free(handle);
    break;
  case UV_TIMER:
    SYNCTRACE("worker_thread_on_(handle)_close - timer\n");
    if( ((uv_timer_t*)handle)->timer_cb==id_conflict_timer_cb ){
      if( handle->data ){
        sqlite3_free(handle->data);
      }
    }
    break;
  default:  /* also to avoid compiler warning about not listed enum elements */
    SYNCTRACE("worker_thread_on_(handle)_close - UNKNOWN TYPE: (%d) %s\n",
              handle->type, uv_handle_type_name(handle->type));
#if TARGET_OS_IPHONE
    //if (uv_is_callback(handle)) {
    //  uv_callback_release((uv_callback_t*) handle);
    //}
#endif
    break;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_walk(uv_handle_t *handle, void *arg) {
  uv_close2(handle, worker_thread_on_close);
}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_peer_message(uv_msg_t *stream, void *msg, int size) {
  node *node;
  int cmd;

  if (size < 0) {
    if (size != UV_EOF) {
      sqlite3_log(SQLITE_ERROR, "read message failed: (%d) %s", size, uv_strerror(size));
    } else {
      SYNCTRACE("worker_thread_on_peer_message - connection dropped\n");
    }
    uv_close2((uv_handle_t*) stream, worker_thread_on_close);  /* disconnect */
    return;
  }

  SYNCTRACE("worker thread received a peer message.  raw size: %d bytes\n", size);

  if( uv_is_closing((uv_handle_t*)stream) ) return;

  node = node_from_socket(stream);

  msg = aergolite_decrypt(node->this_node, (uchar*)msg, &size, 0x4329017E);

  SYNCTRACE("msg size: %d bytes\n", size);

  cmd = binn_map_int32(msg, PLUGIN_CMD);

  switch (cmd) {

  case PLUGIN_CMD_ID:
    SYNCTRACE("   received message: PLUGIN_CMD_ID\n");
    on_node_identification(node, msg, size);
    break;
  case PLUGIN_AUTHORIZATION:
    SYNCTRACE("   received message: PLUGIN_AUTHORIZATION\n");
    on_authorization_received(node, msg, size);
    break;
  case PLUGIN_PEERS:
    SYNCTRACE("   received message: PLUGIN_PEERS\n");
    on_peer_list_received(node, msg, size);
    break;

  case PLUGIN_TEXT: {
    char *text = binn_map_str(msg, PLUGIN_TEXT);
    SYNCTRACE("   received message: PLUGIN_TEXT: %s\n", text);
    on_text_command_received(node, text);
    break;
  }

  /* messages sent to the follower nodes */
  case PLUGIN_ID_CONFLICT:
    SYNCTRACE("   received message: PLUGIN_ID_CONFLICT\n");
    on_id_conflict_recvd(node, msg, size);
    break;
  case PLUGIN_CMD_PING:
    SYNCTRACE("   received message: PLUGIN_CMD_PING\n");
    on_ping_received(node, msg, size);
    break;

  case PLUGIN_DB_PAGE:
    SYNCTRACE("   received message: PLUGIN_DB_PAGE\n");
    on_update_db_page(node, msg, size);
    break;
  case PLUGIN_APPLY_UPDATE:
    SYNCTRACE("   received message: PLUGIN_APPLY_UPDATE\n");
    on_apply_state_update(node, msg, size);
    break;
  case PLUGIN_UPTODATE: // or PLUGIN_IN_SYNC
    SYNCTRACE("   received message: PLUGIN_UPTODATE\n");
    on_in_sync_message(node, msg, size);
    break;

  case PLUGIN_TXN_NOTFOUND:
    SYNCTRACE("   received message: PLUGIN_TXN_NOTFOUND\n");
    on_requested_transaction_not_found(node, msg, size);
    break;
/*
  case PLUGIN_BLOCK_NOTFOUND:
    SYNCTRACE("   received message: PLUGIN_BLOCK_NOTFOUND\n");
    on_requested_block_not_found(node, msg, size);
    break;
*/
  case PLUGIN_REQUESTED_TRANSACTION:
    SYNCTRACE("   received message: PLUGIN_REQUESTED_TRANSACTION\n");
    on_requested_remote_transaction(node, msg, size);
    break;
  case PLUGIN_NEW_TRANSACTION:
    SYNCTRACE("   received message: PLUGIN_NEW_TRANSACTION\n");
    on_new_remote_transaction(node, msg, size);
    break;
  case PLUGIN_NEW_BLOCK:
    SYNCTRACE("   received message: PLUGIN_NEW_BLOCK\n");
    on_new_block(node, msg, size);
    break;
  case PLUGIN_COMMIT_BLOCK:
    SYNCTRACE("   received message: PLUGIN_COMMIT_BLOCK\n");
    on_commit_block(node, msg, size);
    break;
/*
  case PLUGIN_LOG_EXISTS:
    SYNCTRACE("   received message: PLUGIN_LOG_EXISTS\n");
    on_transaction_exists(node, msg, size);
    break;
  case PLUGIN_TRANSACTION_FAILED:
    SYNCTRACE("   received message: PLUGIN_TRANSACTION_FAILED\n");
    on_transaction_failed_msg(node, msg, size);
    break;
*/

  case PLUGIN_GET_MEMPOOL:
    SYNCTRACE("   received message: PLUGIN_GET_MEMPOOL\n");
    on_get_mempool_transactions(node, msg, size);
    break;

  /* messages sent to the leader node */
  case PLUGIN_CMD_PONG:
    SYNCTRACE("   received message: PLUGIN_CMD_PONG\n");
    on_ping_response(node, msg, size);
    break;
  case PLUGIN_REQUEST_STATE_DIFF:
    SYNCTRACE("   received message: PLUGIN_REQUEST_STATE_DIFF\n");
    on_request_state_update(node, msg, size);
    break;
  case PLUGIN_GET_TRANSACTION:
    SYNCTRACE("   received message: PLUGIN_GET_TRANSACTION\n");
    on_get_transaction(node, msg, size);
    break;
/*
  case PLUGIN_GET_BLOCK:
    SYNCTRACE("   received message: PLUGIN_GET_BLOCK\n");
    on_get_block(node, msg, size);
    break;
*/
  case PLUGIN_INSERT_TRANSACTION:
    SYNCTRACE("   received message: PLUGIN_INSERT_TRANSACTION\n");
    on_insert_transaction(node, msg, size);
    break;
  case PLUGIN_NEW_BLOCK_ACK:
    SYNCTRACE("   received message: PLUGIN_NEW_BLOCK_ACK\n");
    on_node_acknowledged_block(node, msg, size);
    break;

  default:
    SYNCTRACE("   ---> unknown received message!  cmd=0x%x  <---\n", cmd);
  }

}

/****************************************************************************/

#if TARGET_OS_IPHONE

SQLITE_PRIVATE void* worker_thread_on_thread_message(uv_callback_t *callback, void *data) {
  plugin *plugin = callback->data;
  int cmd, size = sizeof(int);

  cmd = (int)data;

#else

SQLITE_PRIVATE void worker_thread_on_thread_message(uv_msg_t *stream, void *msg, int size) {
  plugin *plugin;
  int cmd;

  if (size < 0) {
    if (size != UV_EOF) {
      sqlite3_log(SQLITE_ERROR, "read message failed: (%d) %s", size, uv_strerror(size));
    }
    uv_close2((uv_handle_t*) stream, worker_thread_on_close);
    return;
  }

  if (!msg || size==0) {
    SYNCTRACE("worker thread received a command: (null)\n");
    return;
  }

  //loop = ((uv_handle_t*)stream)->loop;
  plugin = (struct plugin*) stream->data;

  cmd = *(int*)msg;

#endif

  SYNCTRACE("worker thread received a command: 0x%x (%d bytes)\n", cmd, size);

  switch (cmd) {
  case WORKER_THREAD_EXIT:
    worker_thread_cmd_exit(plugin->loop);
    break;
  case WORKER_THREAD_NEW_TRANSACTION: {
    worker_thread_on_local_transaction(plugin);
    break;
   }
  }

#if TARGET_OS_IPHONE
  return NULL;
#endif
}

/****************************************************************************/

#if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE==0

SQLITE_PRIVATE void worker_thread_on_pipe_connection(uv_stream_t *server, int status) {
  uv_msg_t *client;
  int rc;

  if (status < 0) {
    sqlite3_log(SQLITE_ERROR, "worker_thread_on_pipe_connection failed: (%d) %s", status, uv_strerror(status));
    return;
  }

  SYNCTRACE("worker_thread_on_pipe_connection - new connection\n");

  /* allocate a new node/peer struct */
  client = sqlite3_malloc(sizeof(uv_msg_t));
  if (!client) { sqlite3_log(SQLITE_ERROR, "worker thread: could not allocate new socket"); return; }

  /* initialize the socket */
  if ((rc = uv_msg_init(server->loop, client, UV_NAMED_PIPE))) {
    sqlite3_log(SQLITE_ERROR, "worker thread: could not initialize pipe socket: (%d) %s", rc, uv_strerror(rc));
    sqlite3_free(client);
    return;
  }

  /* accept the connection */
  if ((rc = uv_accept(server, (uv_stream_t*)client)) == 0) {
    SYNCTRACE("pipe connection accepted\n");
    client->data = server->data;
    /* start reading messages on the connection */
    rc = uv_msg_read_start(client, alloc_buffer, worker_thread_on_thread_message, free_buffer);
    if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not start read: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }
  } else {
    sqlite3_log(SQLITE_ERROR, "worker thread: could not accept connection: (%d) %s", rc, uv_strerror(rc));
loc_failed:
    uv_close2((uv_handle_t*) client, worker_thread_on_close);
  }

}

#endif

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void on_udp_send(uv_udp_send_t *req, int status) {
  if (status) {
    fprintf(stderr, "Send error %s\n", uv_strerror(status));
  }
  sqlite3_free(req);
}

/****************************************************************************/

void *udp_messages = NULL;
void *tcp_messages = NULL;

/****************************************************************************/

SQLITE_PRIVATE void register_udp_message(char *name, udp_message_callback callback){
  struct udp_message *udp_message, new_item = {0};
  int count, pos, i;

  if( udp_messages==NULL ){
    udp_messages = new_array(4, sizeof(struct udp_message));
    if( udp_messages==NULL ) return;
  }

  /* check if already on the array */
  count = array_count(udp_messages);
  for( i=0; i<count; i++ ){
    udp_message = array_get(udp_messages, i);
    if( strcmp(name,udp_message->name)==0 ){
      return;
    }
  }

  /* add it to the array */
  strcpy(new_item.name, name);
  new_item.callback = callback;
  pos = array_append(&udp_messages, &new_item);
  if( pos<0 ){
    SYNCTRACE("register_udp_message FAILED %s\n", name);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void register_tcp_message(char *name, tcp_message_callback callback){
  struct tcp_message *tcp_message, new_item = {0};
  int count, pos, i;

  if( tcp_messages==NULL ){
    tcp_messages = new_array(4, sizeof(struct tcp_message));
    if( tcp_messages==NULL ) return;
  }

  /* check if already on the array */
  count = array_count(tcp_messages);
  for( i=0; i<count; i++ ){
    tcp_message = array_get(tcp_messages, i);
    if( strcmp(name,tcp_message->name)==0 ){
      return;
    }
  }

  /* add it to the array */
  strcpy(new_item.name, name);
  new_item.callback = callback;
  pos = array_append(&tcp_messages, &new_item);
  if( pos<0 ){
    SYNCTRACE("register_tcp_message FAILED %s\n", name);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_udp_message(
  uv_udp_t *socket,
  ssize_t nread,
  const uv_buf_t *buf,
  const struct sockaddr *addr,
  unsigned flags
){
  uv_loop_t *loop = socket->loop;
  plugin *plugin = (struct plugin *) loop->data;
  char ip_address[17] = { 0 }, *command, *arg;
  int i, count;

  if( nread<0 ){
    sqlite3_log(SQLITE_ERROR, "Read error %s\n", uv_err_name(nread));
    uv_close2((uv_handle_t*) socket, worker_thread_on_close);
    goto loc_exit;
  }else if( nread==0 ){
    goto loc_exit;
  }

  uv_ip4_name((const struct sockaddr_in*) addr, ip_address, 16);
  SYNCTRACE("on_udp_message from %s: %.*s len=%u\n", ip_address, (int)nread, buf->base, nread);

  /* if this is a message sent by this same node, discard it */
  if( is_local_ip_address(ip_address) && get_sockaddr_port(addr)==plugin->bind->port ) return;

  command = buf->base;
  arg = stripchr(command, ':');

  count = array_count(udp_messages);
  for( i=0; i<count; i++ ){
    struct udp_message *udp_message = array_get(udp_messages, i);
    if( strcmp(command,udp_message->name)==0 ){
      udp_message->callback(plugin, socket, addr, ip_address, arg);
      break;
    }
  }

loc_exit:

  sqlite3_free(buf->base);

}

/****************************************************************************/

SQLITE_PRIVATE void on_text_command_received(node *node, char *message){
  plugin *plugin = node->plugin;
  char *command, *arg;
  int i, count;

  command = message;
  arg = stripchr(command, ':');

  count = array_count(tcp_messages);
  for( i=0; i<count; i++ ){
    struct tcp_message *tcp_message = array_get(tcp_messages, i);
    if( strcmp(command,tcp_message->name)==0 ){
      tcp_message->callback(plugin, node, arg);
      break;
    }
  }

}

/****************************************************************************/

/*
** This function retrieves the current list of active network interfaces and
** sends the broadcast message to all of them.
*/
SQLITE_PRIVATE int send_local_udp_broadcast(plugin *plugin, char *message) {
  struct sockaddr_in dest_addr;
  uv_udp_send_t *send_req;
  char address_list[128], *address, *next;
  int port, rc = SQLITE_OK;
  uv_buf_t buffer;

  SYNCTRACE("send_local_udp_broadcast - %s\n", message);

  if( !plugin->broadcast ) return SQLITE_INTERNAL;

  port = plugin->broadcast->port;

  get_local_broadcast_address(address_list, sizeof address_list);

  address = address_list;

  while( address && address[0] ){
    next = stripchr(address, ',');

    SYNCTRACE("send_local_udp_broadcast [%s:%d]: %s\n", address, port, message);

    if( uv_ip4_addr(address, port, (struct sockaddr_in *) &dest_addr)!=0 ){
      if( uv_ip6_addr(address, port, (struct sockaddr_in6 *) &dest_addr)!=0 ){
        sqlite3_log(SQLITE_ERROR, "send_local_udp_broadcast: invalid address [%s:%d]", address, port);
        goto loc_next; //return SQLITE_ERROR;
      }
    }

    buffer.len = strlen(message) + 1;
    send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + buffer.len);  /* allocates additional space for the message */
    if( !send_req ) return SQLITE_NOMEM;
    buffer.base = (char*)send_req + sizeof(uv_udp_send_t);
    strcpy(buffer.base, message);

    uv_udp_set_broadcast(plugin->udp_sock, 1);
    rc = uv_udp_send(send_req, plugin->udp_sock, &buffer, 1, (const struct sockaddr *)&dest_addr, on_udp_send);
    if( rc ) rc = SQLITE_ERROR;

loc_next:
    address = next;
  }

  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE int send_udp_broadcast(plugin *plugin, char *message) {
  struct sockaddr_in dest_addr;
  uv_udp_send_t *send_req;
  char *address;
  struct node *node;
  int port, rc = SQLITE_OK;
  uv_buf_t buffer;

  //buffer.base = message;
  //buffer.len = strlen(message) + 1;

  if( plugin->broadcast ){
    return send_local_udp_broadcast(plugin, message);
  }


  //! it should send for the udp broadcast + connected nodes if some of them are outside of the LAN.
  //  it should avoid sending twice for the same node


  /* send the message to all the connected peers */

  for (node = plugin->peers; node; node = node->next) {

    SYNCTRACE("send_udp_broadcast [%s:%d]: %s\n", node->host, node->bind_port, message);

    if( uv_ip4_addr(node->host, node->bind_port, (struct sockaddr_in *) &dest_addr)!=0 ){
      if( uv_ip6_addr(node->host, node->bind_port, (struct sockaddr_in6 *) &dest_addr)!=0 ){
        sqlite3_log(SQLITE_ERROR, "send_udp_broadcast: invalid address [%s:%d]", node->host, node->port);
        continue; //return SQLITE_ERROR;
      }
    }

    buffer.len = strlen(message) + 1;
    send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + buffer.len);  /* allocates additional space for the message */
    if (!send_req) return SQLITE_NOMEM;
    buffer.base = (char*)send_req + sizeof(uv_udp_send_t);
    strcpy(buffer.base, message);

    rc = uv_udp_send(send_req, plugin->udp_sock, &buffer, 1, (const struct sockaddr *)&dest_addr, on_udp_send);
    if( rc ){
      sqlite3_log(SQLITE_ERROR, "send_udp_broadcast: failed sending to [%s:%d]", node->host, node->bind_port);
    }

  }

  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE int send_tcp_broadcast(plugin *plugin, char *message) {
  struct node *node;
  int rc = SQLITE_OK;

  /* send the message to all the connected peers */

  for (node = plugin->peers; node; node = node->next) {
    BOOL ret;

    if( !node->is_authorized ) continue;
    SYNCTRACE("send_tcp_broadcast [%s:%d]: %s\n", node->host, node->port, message);

    ret = send_text_message(node, message);
    if( ret==FALSE ){
      sqlite3_log(SQLITE_ERROR, "send_tcp_broadcast: failed sending to [%s:%d]", node->host, node->port);
    }
  }

  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE int send_udp_message(plugin *plugin, const struct sockaddr *address, char *message) {
  uv_udp_send_t *send_req;
  uv_buf_t buffer;
  int rc = SQLITE_OK;

  if( !address ) return SQLITE_INTERNAL;

#ifdef DEBUGPRINT
  {
  char ip_address[20];
  int port = get_sockaddr_port(address);
  get_ip_address(address, ip_address, 16);
  SYNCTRACE("send_udp_message [%s:%d]: %s\n", ip_address, port, message);
  }
#endif

  buffer.len = strlen(message) + 1;
  send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + buffer.len);  /* allocates additional space for the message */
  if (!send_req) return SQLITE_NOMEM;
  buffer.base = (char*)send_req + sizeof(uv_udp_send_t);
  strcpy(buffer.base, message);

  rc = uv_udp_send(send_req, plugin->udp_sock, &buffer, 1, address, on_udp_send);
  if( rc ) rc = SQLITE_ERROR;
  return rc;

}

/****************************************************************************/

SQLITE_PRIVATE int send_udp_message_ex(plugin *plugin, char *ip_addr, int port, char *message) {
  const struct sockaddr_in6 *address;

  if( uv_ip4_addr(ip_addr, port, (struct sockaddr_in *) &address)!=0 ){
    if( uv_ip6_addr(ip_addr, port, (struct sockaddr_in6 *) &address)!=0 ){
      sqlite3_log(SQLITE_ERROR, "send_udp_message: invalid address [%s:%d]", ip_addr, port);
      return SQLITE_ERROR;
    }
  }

  return send_udp_message(plugin, (const struct sockaddr*) address, message);
}

/****************************************************************************/
/****************************************************************************/

#include "node_discovery.c"
#include "leader_election.c"

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void node_thread(void *arg) {
  plugin *plugin = (struct plugin *) arg;
  aergolite *this_node;
  uv_loop_t loop;
#if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE==0
  uv_msg_t insock;
#endif
  struct tcp_address *address;
  int rc;

  SYNCTRACE("worker thread running\n");

  if( !plugin ) return;
  this_node = plugin->this_node;

  uv_loop_init(&loop);

  /* save the address to the plugin struct in the loop struct */
  loop.data = plugin;
  /* save the address to the event loop in the plugin struct */
  plugin->loop = &loop;

  /* initialize sub-modules */
  node_discovery_init();
  leader_election_init();

  /* register UDP message handlers */
  /* a broadcast message requesting blockchain status info */
  register_udp_message("blockchain_status", on_blockchain_status_request);
  /* a broadcast message requesting consensus protocol status info */
  register_udp_message("protocol_status", on_protocol_status_request);
  /* a broadcast message requesting node info */
  register_udp_message("node_info", on_node_info_request);

  /* new node info from peer */
  register_tcp_message("node_info", on_peer_info_changed);


#if TARGET_OS_IPHONE
  /* initialize a callback to receive notifications from the main thread */
  uv_callback_init(&loop, &plugin->worker_cb, worker_thread_on_thread_message, UV_DEFAULT);
  plugin->worker_cb.data = this_node;
#else
  /* create a socket to communicate with the caller thread */
  rc = uv_msg_init(&loop, &insock, UV_NAMED_PIPE);
  if( rc < 0 ){ sqlite3_log(SQLITE_ERROR, "cannot initialize socket on worker thread"); goto loc_return; }

  /* save the address to the plugin struct in the stream/socket struct */
  insock.data = plugin;

  SYNCTRACE("binding worker thread socket to address %s\n", plugin->worker_address);
  if ((rc = uv_pipe_bind((uv_pipe_t*) &insock, plugin->worker_address))) {
    sqlite3_log(SQLITE_ERROR, "worker thread: bind to pipe [%s] failed: (%d) %s", plugin->worker_address, rc, uv_strerror(rc));
    goto loc_return;
  }

  SYNCTRACE("listening on socket\n");
  if ((rc = uv_listen((uv_stream_t*) &insock, 128, worker_thread_on_pipe_connection))) {
    sqlite3_log(SQLITE_ERROR, "worker thread: listen on pipe socket [%s] failed: (%d) %s", plugin->worker_address, rc, uv_strerror(rc));
    goto loc_return;
  }
#endif



  /* connection to the other nodes */

  /* bind to the supplied TCP address(es) */
  for (address = plugin->bind; address; address = address->next) {
    struct sockaddr_storage addr;
    uv_tcp_t *server;
    //uv_udp_t *udp_sock;
    SYNCTRACE("peer connections - binding to address: %s:%d\n", address->host, address->port);

    if( uv_ip4_addr(address->host, address->port, (struct sockaddr_in *) &addr)!=0 ){
      if( uv_ip6_addr(address->host, address->port, (struct sockaddr_in6 *) &addr)!=0 ){
        sqlite3_log(SQLITE_ERROR, "worker thread: invalid address [%s:%d]", address->host, address->port);
        goto loc_failed;
      }
    }

    server = sqlite3_malloc(sizeof(uv_tcp_t));
    if (!server) goto loc_no_memory;
    uv_tcp_init(&loop, server);
    if( (rc = uv_tcp_bind(server, (const struct sockaddr*) &addr, 0)) ){
      sqlite3_log(SQLITE_ERROR, "worker thread: bind TCP socket to address [%s:%d] failed: (%d) %s", address->host, address->port, rc, uv_strerror(rc));
      //sqlite3_free(server);
      goto loc_failed;
    }
    if( (rc = uv_listen((uv_stream_t*) server, DEFAULT_BACKLOG, worker_thread_on_incoming_tcp_connection)) ){
      sqlite3_log(SQLITE_ERROR, "worker thread: listen on TCP socket [%s:%d] failed: (%d) %s", address->host, address->port, rc, uv_strerror(rc));
      //sqlite3_free(server);
      goto loc_failed;
    }

    if( address->port==0 ){
      int addrlen = sizeof(struct sockaddr_storage);
      uv_tcp_getsockname(server, (struct sockaddr*) &addr, &addrlen);
      address->port = get_sockaddr_port((struct sockaddr*) &addr);
      SYNCTRACE("bound to port %d\n", address->port);
    }

    plugin->udp_sock = sqlite3_malloc_zero(sizeof(uv_udp_t));
    if (!plugin->udp_sock) goto loc_no_memory;
    uv_udp_init(&loop, plugin->udp_sock);
    if( (rc = uv_udp_bind(plugin->udp_sock, (const struct sockaddr*) &addr, UV_UDP_REUSEADDR)) ){
      sqlite3_log(SQLITE_ERROR, "worker thread: bind UDP socket to address [%s:%d] failed: (%d) %s", address->host, address->port, rc, uv_strerror(rc));
      //sqlite3_free(plugin->udp_sock);
      goto loc_failed;
    }
    if( (rc = uv_udp_recv_start(plugin->udp_sock, alloc_buffer, on_udp_message)) ){
      sqlite3_log(SQLITE_ERROR, "worker thread: UDP socket recv start at address [%s:%d] failed: (%d) %s", address->host, address->port, rc, uv_strerror(rc));
      //sqlite3_free(plugin->udp_sock);
      goto loc_failed;
    }
  }


  /* initialize the timers */
  uv_timer_init(&loop, &plugin->after_connections_timer);
  uv_timer_init(&loop, &plugin->leader_check_timer);
  uv_timer_init(&loop, &plugin->election_info_timer);
  uv_timer_init(&loop, &plugin->reconnect_timer);

  uv_timer_init(&loop, &plugin->process_transactions_timer);
  uv_timer_init(&loop, &plugin->new_block_timer);

  uv_timer_init(&loop, &plugin->aergolite_core_timer);


  start_node_discovery(plugin);

  if( plugin->udp_sock ){
    /* start the after connections timer */
    SYNCTRACE("starting the after connections timer\n");
    uv_timer_start(&plugin->after_connections_timer, after_connections_timer_cb, 1500, 0);
  }


  /* load the database state */
  load_current_state(plugin);


  /* mark this thread as active */
  plugin->thread_active = TRUE;
  SYNCTRACE("the worker thread is active\n");

  /* run the message loop for this thread */
  uv_run(&loop, UV_RUN_DEFAULT);

  /* mark this thread as closing */
  plugin->thread_active = FALSE;

loc_exit:

  /* close the handles and the loop */
  SYNCTRACE("cleaning up worker thread - 1\n");
  /* close all the timers first and wait for the close callbacks to be fired */
  //uv_timer_stop(&plugin->log_rotation_timer);
  //uv_close2((uv_handle_t *) &plugin->log_rotation_timer, NULL);

#if TARGET_OS_IPHONE
  //uv_callback_stop(&plugin->worker_cb);
  uv_callback_stop_all(&loop);
#else
  uv_close2((uv_handle_t *) &insock, NULL);  /* this one should not be freed, so it is closed before without a callback */
#endif
  uv_run(&loop, UV_RUN_NOWAIT);  /* or UV_RUN_ONCE */

  SYNCTRACE("cleaning up worker thread - 2\n");
  uv_walk(&loop, worker_thread_on_walk, NULL);
  uv_run(&loop, UV_RUN_DEFAULT);
  uv_loop_close(&loop);

loc_return:
  plugin->thread_running = FALSE;
  SYNCTRACE("worker thread: returning\n");
  return;

loc_no_memory:
  sqlite3_log(SQLITE_NOMEM, "worker thread: no memory");

loc_failed:
  sqlite3_log(rc, "worker thread: failed");
  uv_stop(&loop);
  goto loc_exit;

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void close_worker_thread(plugin *plugin) {
  int cmd, rc;

  SYNCTRACE("close_worker_thread\n");

  if( !plugin->thread_running ) return;

  /* the worker thread event loop must be running for it ro receive the message */
  while( !plugin->thread_active ) sqlite3_sleep(25);

  /* send command to the worker thread */
  SYNCTRACE("sending worker thread command: exit\n");

  cmd = WORKER_THREAD_EXIT;
  rc = send_notification_to_worker(plugin, (char*)&cmd, sizeof(cmd));
  if( rc<0 ){
    SYNCTRACE("send_message failed: (%d) %s\n", rc, uv_strerror(rc));
    return;
  }

  /* wait for the thread to exit */
  SYNCTRACE("waiting for the worker thread to exit\n");
  uv_thread_join(&plugin->worker_thread);
  assert(plugin->thread_active==FALSE);
  assert(plugin->thread_running==FALSE);
  SYNCTRACE("the worker thread is closed\n");

}

/****************************************************************************/

SQLITE_API void plugin_end(void *arg){
  plugin *plugin = (struct plugin *) arg;

  /* close the worker thread with all the connections */
  close_worker_thread(plugin);

  clear_leader_votes(plugin);

  while( plugin->mempool ){
    discard_mempool_transaction(plugin, plugin->mempool);
  }

  while( plugin->bind ){
    struct tcp_address *next = plugin->bind->next;
    sqlite3_free(plugin->bind);
    plugin->bind = next;
  }

  while( plugin->discovery ){
    struct tcp_address *next = plugin->discovery->next;
    sqlite3_free(plugin->discovery);
    plugin->discovery = next;
  }

  discard_block(plugin->current_block);
  discard_block(plugin->new_block);

  sqlite3_mutex_free(plugin->mutex);

  sqlite3_free(plugin);

}

/****************************************************************************/
/****************************************************************************/

#define DEFAULT_RECONNECT_INTERVAL   3000  // milliseconds = 3 seconds

SQLITE_PRIVATE struct tcp_address * parse_tcp_address(char *address, int common_reconnect_interval) {
  struct tcp_address *first=0, *prev=0, *addr=0;
  char *base;

  SYNCTRACE("parse_tcp_address %s\n", address);

  if( !address ) return NULL;

  base = address = sqlite3_strdup(address);

  while( address ){
    char *host, *zport, *next;

    next = stripchr(address, ',');

    if( strncmp(address, "tcp://", 6)==0 ){
      host = address + 6;
    }else{
      host = address;
#if 0
      sqlite3_log(SQLITE_ERROR, "the protocol is not supported: %s", address);
      goto loc_failed;
#endif
    }

    addr = sqlite3_malloc(sizeof(struct tcp_address));
    if( !addr ){
      sqlite3_log(SQLITE_NOMEM, "out of memory");
      goto loc_failed;
    }
    memset(addr, 0, sizeof(struct tcp_address));

    if( !first ) first = addr;
    if( prev ) prev->next = addr;
    prev = addr;

    if( *host=='[' ){
      zport = stripchr(host, ']');
      host++;
      if( zport && *zport==':' ){
        zport++;
      } else {
        zport = 0;
      }
    } else {
      zport = stripchr(host, ':');
    }
    if( !zport ){
      char *p;
      for(p=host; *p; p++){
        if( !isdigit(*p) ){
          sqlite3_log(SQLITE_ERROR, "the port must be informed: %s", address);
          goto loc_failed;
        }
      }
      zport = host;
      host = "0.0.0.0";
    }
    addr->port = atoi(zport);

    if (strlen(host) > sizeof(addr->host) - 1) {
      sqlite3_log(SQLITE_ERROR, "the host name is too long: %s", host);
      goto loc_failed;
    }
    strcpy(addr->host, host);

    if (common_reconnect_interval > 0) {
      addr->reconnect_interval = common_reconnect_interval;
    } else {
      addr->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
    }

    address = next;
  }

  sqlite3_free(base);
  return first;
loc_failed:
  addr = first;
  while( addr ){
    struct tcp_address *next = addr->next;
    sqlite3_free(addr);
    addr = next;
  }
  sqlite3_free(base);
  return (struct tcp_address *)-1;
}

/****************************************************************************/

SQLITE_PRIVATE struct tcp_address * parse_discovery_address(char *address, int common_reconnect_interval) {
  struct tcp_address *first=0, *prev=0, *addr=0;
  char *base;

  SYNCTRACE("parse_discovery_address %s\n", address);

  if( !address ) return NULL;

  base = address = sqlite3_strdup(address);

  while( address ){
    char *host, *zport, *next;

    next = stripchr(address, ',');

    addr = sqlite3_malloc_zero(sizeof(struct tcp_address));
    if( !addr ){
      sqlite3_log(SQLITE_NOMEM, "out of memory");
      goto loc_failed;
    }

    if( !first ) first = addr;
    if( prev ) prev->next = addr;
    prev = addr;

    host = address;

    if( *host=='[' ){
      zport = stripchr(host, ']');
      host++;
      if( zport && *zport==':' ){
        zport++;
      } else {
        zport = 0;
      }
    } else {
      zport = stripchr(host, ':');
    }
    if (!zport) {
      sqlite3_log(SQLITE_ERROR, "the port must be informed: %s", address);
      goto loc_failed;
    }
    addr->port = atoi(zport);

    if (strlen(host) > sizeof(addr->host) - 1) {
      sqlite3_log(SQLITE_ERROR, "the host name is too long: %s", host);
      goto loc_failed;
    }
    if( strncmp(host, "local", 5)==0 ){
      addr->is_broadcast = 1;
    }else{
      strcpy(addr->host, host);
    }

    if (common_reconnect_interval > 0) {
      addr->reconnect_interval = common_reconnect_interval;
    } else {
      addr->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
    }

    address = next;
  }

  sqlite3_free(base);
  return first;
loc_failed:
  addr = first;
  while( addr ){
    struct tcp_address *next = addr->next;
    sqlite3_free(addr);
    addr = next;
  }
  sqlite3_free(base);
  return (struct tcp_address *)-1;
}

/****************************************************************************/

void * plugin_init(aergolite *this_node, char *uri) {
  struct tcp_address *addr;
  plugin *plugin;
  char *discovery, *bind, *block_interval;
  int64 random_no;

  SYNCTRACE("initializing a new instance of mini-raft plugin\n");

  /* allocate a new plugin object */

  plugin = sqlite3_malloc_zero(sizeof(struct plugin));
  if( !plugin ) return NULL;  //SQLITE_NOMEM;

  plugin->this_node = this_node;

  plugin->mutex = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);


  plugin->node_id = aergolite_get_node_id(this_node);

  plugin->pubkey = aergolite_pubkey(this_node, &plugin->pklen);


  plugin->is_leader = FALSE;


  /* parse the node discovery parameter */

  discovery = (char*) sqlite3_uri_parameter(uri, "discovery");
  /*
  ** if no node discovery is supplied, the database can only be used locally
  ** without connections to other nodes.
  */
  if( discovery ){
    plugin->discovery = parse_discovery_address(discovery, 0);
    if( plugin->discovery==(struct tcp_address *)-1 ) goto loc_failed;
    for (addr = plugin->discovery; addr; addr = addr->next) {
      SYNCTRACE("  discovery address: %s:%d \n", addr->host, addr->port);
      if( addr->is_broadcast ) plugin->broadcast = addr;
    }
  }

  /* parse the bind parameter */

  bind = (char*) sqlite3_uri_parameter(uri, "bind");
  /*
  ** if no bind address is supplied, it:
  ** -use the same address of node discovery (if using local UDP broadcast)
  ** -binds to a random port  -- must be the same as the TCP port?
  */
  if( bind ){
    plugin->bind = parse_tcp_address(bind, 0);
    if( plugin->bind==(struct tcp_address *)-1 ) goto loc_failed;
  }else if( plugin->broadcast ){
    plugin->bind = sqlite3_memdup(plugin->broadcast, sizeof(struct tcp_address));
    if( plugin->bind ) plugin->bind->next = NULL;
    strcpy(plugin->bind->host, "0.0.0.0");
  }else{
    /* bind to random port */
    plugin->bind = sqlite3_malloc_zero(sizeof(struct tcp_address));
    if( !plugin->bind ) goto loc_failed;
    strcpy(plugin->bind->host, "0.0.0.0");
  }
  for (addr = plugin->bind; addr; addr = addr->next) {
    SYNCTRACE("  bind address: %s:%d \n", addr->host, addr->port);
  }


  /* parse the block interval parameter */

  plugin->block_interval = -1;
  block_interval = (char*) sqlite3_uri_parameter(uri, "block_interval");
  if( block_interval ){
    plugin->block_interval = atoi(block_interval);
  }
  if( plugin->block_interval<0 ){
    plugin->block_interval = NEW_BLOCK_WAIT_INTERVAL;
  }


  /* set the unix domain socket or pipe used to communicate with the worker thread */

  //sqlite3_randomness(0, NULL); /* reseed the PRNG */
  do {
    sqlite3_randomness(sizeof(int64), &random_no);
  } while (random_no==0);

#if TARGET_OS_IPHONE
  /* it is done in the node_thread */
#elif defined(_WIN32)
  sprintf(plugin->worker_address, "\\\\?\\pipe\\aergolite%" UINT64_FORMAT, random_no);
#elif defined(__ANDROID__)
  //sprintf(plugin->worker_address, "/data/local/tmp/aergolite%" UINT64_FORMAT, random_no);
  sprintf(&plugin->worker_address[2], "aergolite%" UINT64_FORMAT, random_no);
  plugin->worker_address[0] = 0;
  plugin->worker_address[1] = strlen(&plugin->worker_address[2]);
  //sprintf(buf, "aergolite%" UINT64_FORMAT, random_no);
  //if (uv_build_abstract_socket_name(buf, strlen(buf), plugin->worker_address) == NULL) ...
#else
  sprintf(plugin->worker_address, "/tmp/aergolite%" UINT64_FORMAT, random_no);
  /* remove the file if it already exists */
  unlink(plugin->worker_address);
#endif

#if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE==0
  SYNCTRACE("worker thread socket address: %s\n", plugin->worker_address);
#endif

  /* ignore SIGPIPE signals */
  signal(SIGPIPE, SIG_IGN);

  /* start the worker thread */
  SYNCTRACE("starting worker thread\n");
  uv_thread_create(&plugin->worker_thread, node_thread, plugin);
  plugin->thread_running = TRUE;

  //send_request_to_worker(plugin->worker_address, msg, size, &response);

  return plugin;

loc_failed:

  sqlite3_free(plugin);
  return NULL;

}

/****************************************************************************/

// int register_plugin(){ -- if using as a library, this can be the entry point

int register_miniraft_plugin(){
  int rc;

  SYNCTRACE("registering the mini-raft plugin\n");

  rc = aergolite_plugin_register("mini-raft",
    plugin_init,
    plugin_end,
    on_new_local_transaction,
    get_protocol_status,
    on_local_node_info_changed,
    print_node_list
  );

  return rc;
}
