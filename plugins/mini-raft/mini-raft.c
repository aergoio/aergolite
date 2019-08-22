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

    for (node = plugin->peers; node; node = node->next) {
      if (node->conn_state != CONN_STATE_CONNECTED) continue;
      if( node != plugin->peers ){
        sqlite3_str_appendchar(str,1, ',');
      }
      sqlite3_str_appendall(str, "{\n");

      sqlite3_str_appendf(str, "  \"node_id\": %d,\n", node->id);
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
      num_transactions++;
      last = txn;
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

/***************************************************************************/
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

  check_current_leader(plugin);

}

/****************************************************************************/

#ifdef _WIN32
#define PERIODIC_INTERVAL 1000
#else
#define PERIODIC_INTERVAL 3000
#endif

SQLITE_PRIVATE void aergolite_core_timer_cb(uv_timer_t* handle){
  plugin *plugin = (struct plugin *) handle->loop->data;

  if( !plugin->is_updating_state ){
    aergolite_periodic(plugin->this_node);
  }

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
/****************************************************************************/

#if 0

// retrieve it from the mempool or, if not found, from the db (if full node)

SQLITE_PRIVATE void on_get_transaction(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int64 tid, nonce;
  binn *map;
  void *log=0;
  int rc, node_id;

  tid = binn_map_int64(msg, PLUGIN_TID);

  SYNCTRACE("on_get_transaction - request from node %d - tid=%" INT64_FORMAT "\n", node->id, tid);

  map = binn_map();
  if (!map) goto loc_failed;

  /* check if it is in the mempool */
  ...


  /* load it from the database */
  rc = aergolite_get_transaction(this_node, tid, &node_id, &nonce, &log);

  switch( rc ){
  case SQLITE_NOTFOUND: /* there is no record with the given prev_tid */
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_LOG_NOTFOUND);
    break;
  case SQLITE_OK:
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUESTED_TRANSACTION);
    binn_map_set_int64(map, PLUGIN_TID, tid);
    binn_map_set_int32(map, PLUGIN_NODE_ID, node_id);
    binn_map_set_int64(map, PLUGIN_NONCE, nonce);
    binn_map_set_list (map, PLUGIN_SQL_CMDS, log);
    sqlite3_free(log);
    break;
  default:
    sqlite3_log(rc, "on_get_transaction: get_next_blockchain_txn failed");
    goto loc_failed;
  }

  send_peer_message(node, map, on_data_sent);

  return;

loc_failed:

  if (map) binn_free(map);

}

#endif

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
/*** ALLOWED NODES **********************************************************/
/****************************************************************************/

#if 0
/*
** Check if the node is allowed to participate in the network
*/
SQLITE_PRIVATE int check_if_allowed_node(plugin *plugin, int id, int *pis_allowed) {
  aergolite *this_node = plugin->this_node;
  int count, rc;

  SYNCTRACE("check_if_allowed_node id=%d\n", id);

//! should it check the public key?
//! should it use some authentication?

// this function could be in the core

  /* check if already in the list of known nodes */
  rc = aergolite_consensus_db_query_int32(this_node, pis_allowed,
         "SELECT count(*) FROM aergolite_allowed_nodes WHERE id=%d", id);
  if( rc ) return;

  return rc;
}
#endif

/****************************************************************************/

/*
** Notes:
** This function is reading from the main_db2.
** It is currently NOT informing other nodes about the total_known_nodes.
*/
SQLITE_PRIVATE void update_known_nodes(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  node *node;

  SYNCTRACE("update_known_nodes\n");

  /* check if nodes already exist in the list of known nodes */

  plugin->total_known_nodes = 1; // this node

  //add_known_node(plugin, plugin->node_id);

  for( node=plugin->peers; node; node=node->next ){
    //add_known_node(plugin, node->id);
    plugin->total_known_nodes++;
  }

  /* the leader must know the number of total known nodes, including those that are off-line */
//  aergolite_queue_db_query_int32(this_node, &plugin->total_known_nodes, "SELECT count(*) FROM aergolite_allowed_nodes");

  SYNCTRACE("update_known_nodes total_known_nodes=%d\n", plugin->total_known_nodes);

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_accepted_node(node *node) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_new_accepted_node\n");

  if( plugin->is_leader ){
    update_known_nodes(plugin);
  }

}

/****************************************************************************/
/* PEER FUNCTIONS ***********************************************************/
/****************************************************************************/

SQLITE_PRIVATE void stop_id_conflict_timer(struct node_id_conflict *id_conflict) {

  id_conflict->existing_node->id_conflict = NULL;
  id_conflict->new_node->id_conflict = NULL;

  uv_timer_stop(&id_conflict->timer);
  uv_close2( (uv_handle_t*) &id_conflict->timer, worker_thread_on_close);

}

SQLITE_PRIVATE void id_conflict_timer_cb(uv_timer_t* handle) {
  //aergolite *this_node = (aergolite *) handle->loop->data;
  struct node_id_conflict *id_conflict = (struct node_id_conflict *) handle->data;
  node *existing_node, *new_node;

  // 5. if the timer is fired before the answer:
  //        1. close the old connection
  //        2. continue with the identification process with the new node

  existing_node = id_conflict->existing_node;
  new_node = id_conflict->new_node;

  stop_id_conflict_timer(id_conflict);

  disconnect_peer(existing_node);

  /* start the db sync */
  on_new_accepted_node(new_node);

}

SQLITE_PRIVATE void on_id_conflict_recvd(node *node, void *msg, int size) {

  sqlite3_log(1, "this node has the same node id as another one");

}

SQLITE_PRIVATE void on_id_conflict_sent(send_message_t *req, int status) {

  /* disconnect the peer */
  uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);

}

SQLITE_PRIVATE void on_ping_received(node *node, void *msg, int size) {
  binn *map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_PONG) == FALSE ||
      send_peer_message(node, map, NULL) == FALSE )
  {
    sqlite3_log(1, "on_ping_received: send_peer_message failed");
  }
  if (map) binn_free(map);
}

SQLITE_PRIVATE void on_ping_response(node *node, void *msg, int size) {
  struct node_id_conflict *id_conflict;
  binn *map;

  id_conflict = node->id_conflict;
  if( !id_conflict ) return;

  /* send a message to the new node informing about the conflict */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_ID_CONFLICT) == FALSE ||
      send_peer_message(id_conflict->new_node, map, on_id_conflict_sent) == FALSE )
  {
    sqlite3_log(1, "on_ping_response: send_peer_message failed");
  }
  if (map) binn_free(map);

  /* stop the timer */
  stop_id_conflict_timer(id_conflict);

}

SQLITE_PRIVATE void on_new_node_with_same_id(node *existing_node, node *new_node) {
  plugin *plugin = existing_node->plugin;
  aergolite *this_node = existing_node->this_node;
  struct node_id_conflict *id_conflict;
  binn *map;

  sqlite3_log(1, "new connected node has the same node id as another one");

  // 1. send a packet to the already connected node
  // 2. save info on a struct
  // 3. start a timer and pass this struct
  // 4. if the node answers before the timer:
  //        1. stop the timer
  //        2. send a message to the new connection informing about the colision
  //        3. close the new connection when the packet is sent
  // 5. if the timer is fired before the answer:
  //        1. close the old connection
  //        2. continue with the identification process with the new node

  // or send the msg to all the nodes with this id?

  //! this implementation is not perfect
  //  if 2 new nodes connect with the same node_id, the behavior is undefined

  /* send a packet to the already connected node */
  map = binn_map();
  if (binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_PING) == FALSE) goto loc_failed1;
  if (send_peer_message(existing_node, map, NULL) == FALSE) goto loc_failed1;
  binn_free(map); map = 0;

  /* save the conflict information */
  id_conflict = sqlite3_malloc_zero(sizeof(struct node_id_conflict));
  if( !id_conflict ) goto loc_failed2;
  id_conflict->existing_node = existing_node;
  id_conflict->new_node = new_node;

  existing_node->id_conflict = id_conflict;
  new_node->id_conflict = id_conflict;
//--
  //id_conflict->next = plugin->node_id_conflicts;
  //plugin->node_id_conflicts = id_conflict;
//++

  /* initialize the node id conflict timer */
  SYNCTRACE("starting the id conflict timer\n");
  uv_timer_init(plugin->loop, &id_conflict->timer);
  id_conflict->timer.data = id_conflict;  /* release on timer close */
  uv_timer_start(&id_conflict->timer, id_conflict_timer_cb, 3000, 0);  /* wait 3 seconds */

  return;
loc_failed1:
  sqlite3_log(1, "on_new_node_with_same_id: send_peer_message failed");
  if (map) binn_free(map);
loc_failed2:
  disconnect_peer(existing_node);

}

SQLITE_PRIVATE void check_new_node_id(node *node) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  struct node *tnode;

  /* check for node id conflict with this node */
  if (node->id == plugin->node_id) {
    sqlite3_log(1, "new connected node has the same node id as this node");
    goto loc_invalid_peer;
  }

  /* check for node id conflict with other node */
  for (tnode = plugin->peers; tnode; tnode = tnode->next) {
    if (tnode!=node && tnode->id == node->id) {
      on_new_node_with_same_id(tnode, node);
      return;
    }
  }

  /* start the db sync */
  on_new_accepted_node(node);

  return;

loc_invalid_peer:
  /* disconnect the peer */
  disconnect_peer(node);

}

/****************************************************************************/

#if 0

SQLITE_PRIVATE void on_new_id_request_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_new_id_request_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_id_received(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int node_id;

  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);

  if( node_id > 0 ){
    /* save the node id in the local config */
    int rc = aergolite_set_node_config_int(this_node, "node_id", node_id);
    if( rc ){
      sqlite3_log(rc, "on_new_node_id_received: could not save the new node id in the config");
      /* disconnect the peer */
      disconnect_peer(node);
      return;
    }
    /* store it in the struct */
    plugin->node_id = node_id;
    /* check if valid and start db sync */
    check_new_node_id(node);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_id_request(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int  rc, node_id, max_node_id=0;
  binn *map=0;

  /* get a new node id */
  node_id = aergolite_get_node_config_int(this_node, "last_node_id");
  if( node_id > 1 ){
    node_id++;
  } else {
    /* get an unused node id */
    struct node *tnode;
    for (tnode = plugin->peers; tnode; tnode = tnode->next) {
      if (tnode->id > max_node_id) max_node_id = tnode->id;
    }
    if( max_node_id==0 ) max_node_id = 1;
    node_id = max_node_id + 1;
  }
  if( node_id==plugin->node_id ) node_id++;
  rc = aergolite_set_node_config_int(this_node, "last_node_id", node_id);
  if( rc ){
    sqlite3_log(rc, "on_new_node_id_request: could not save the new node id in the config");
    goto loc_failed;
  }

  /* send it to the secondary node */
  map = binn_map();
  if (binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_NODE_ID) == FALSE) goto loc_failed;
  if (binn_map_set_int32(map, PLUGIN_NODE_ID, node_id) == FALSE) goto loc_failed;
  if (send_peer_message(node, map, on_new_id_request_sent) == FALSE) {
    sqlite3_log(1, "on_new_node_id_request: send_peer_message failed");
loc_failed:
    if (map) binn_free(map);
    disconnect_peer(node);
    return;
  }

  /* save the node id in the node struct */
  node->id = node_id;

  check_new_node_id(node);

}

#endif

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_identified(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;

  node->id = binn_map_int32(msg, PLUGIN_NODE_ID);

  SYNCTRACE("remote node identified - node_id=%d\n", node->id);

  /* if the node id was not supplied */
  if( plugin->node_id==0 ){
    sqlite3_log(1, "on_new_node_identified: empty node id!");
    disconnect_peer(node);
  } else if( node->id > 0 ){
    /* xxx */
    check_new_node_id(node);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_id_msg_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_id_msg_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void on_new_node_connected(node *node) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  binn *map=0;

  if (node == NULL) return;

  SYNCTRACE("new node connected: %s:%d ---\n", node->host, node->port);

  // it could call a user callback function to ... identify/authenticate it, or simply to log


  // maybe they should authenticate themselves before the db sync (later)


  /* send the identification to the other peer */
  map = binn_map();
  if (!map) goto loc_failed;

  if (binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_ID) == FALSE) goto loc_failed;
  if (binn_map_set_int32(map, PLUGIN_NODE_ID, plugin->node_id) == FALSE) goto loc_failed;

  if (send_peer_message(node, map, on_id_msg_sent) == FALSE) {
    sqlite3_log(1, "on_new_node_connected: send_peer_message failed");
    goto loc_failed;
  }

loc_exit:
  if (map) binn_free(map);
  return;

loc_failed:
  sqlite3_log(1, "on_new_node_connected: binn failed. probably out of memory");
  disconnect_peer(node);
  goto loc_exit;

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

  if( node==plugin->leader_node ){
    plugin->leader_node = NULL;
    plugin->sync_down_state = DB_STATE_UNKNOWN;
    if( plugin->sync_up_state==DB_STATE_SYNCHRONIZING ){
      plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
    }
    uv_timer_stop(&plugin->process_transactions_timer);
    uv_timer_stop(&plugin->new_block_timer);
    if( plugin->thread_active ){
      enable_reconnect_timer(plugin);
      //start_leader_election(plugin);
    }
  }

  if( node==plugin->last_leader ){
    plugin->last_leader = NULL;
  }

  // it can log, notify the app, reconnect

}

/****************************************************************************/

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

#define leader_reconnect_interval 250

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

  switch (handle->type) {
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
      sqlite3_free(node);

      //enable_node_reconnect_timer(node); //! it can activate a timer when there is no more peers

    } else {  /* this is the listening socket */
      sqlite3_free(handle);
    }
    break;
   }
  case UV_UDP:
    sqlite3_free(handle);
    break;
  case UV_TIMER:
    if( ((uv_timer_t*)handle)->timer_cb==id_conflict_timer_cb ){
      if( handle->data ){
        sqlite3_free(handle->data);
      }
    }
    break;
  default:  /* also to avoid compiler warning about not listed enum elements */
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
    on_new_node_identified(node, msg, size);
    break;

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

/*
  case PLUGIN_TXN_NOTFOUND:
    SYNCTRACE("   received message: PLUGIN_TXN_NOTFOUND\n");
    on_requested_transaction_not_found(node, msg, size);
    break;
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

  /* messages sent to the leader node */
  case PLUGIN_CMD_PONG:
    SYNCTRACE("   received message: PLUGIN_CMD_PONG\n");
    on_ping_response(node, msg, size);
    break;
  case PLUGIN_REQUEST_STATE_DIFF:
    SYNCTRACE("   received message: PLUGIN_REQUEST_STATE_DIFF\n");
    on_request_state_update(node, msg, size);
    break;
/*
  case PLUGIN_GET_TRANSACTION:
    SYNCTRACE("   received message: PLUGIN_GET_TRANSACTION\n");
    on_get_transaction(node, msg, size);
    break;
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
    SYNCTRACE("   unknown received message!\n");
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
    uv_close2((uv_handle_t*) stream, NULL);
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
/* process either incoming or outgoing connections */
SQLITE_PRIVATE int worker_thread_on_tcp_connection(node *node) {
  int rc;

  /* start reading messages on the connection */
  rc = uv_msg_read_start(&node->socket, alloc_buffer, worker_thread_on_peer_message, free_buffer);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not start read: (%d) %s", rc, uv_strerror(rc)); return rc; }

  rc = uv_tcp_nodelay( (uv_tcp_t*) &node->socket, 1);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not set TCP no delay: (%d) %s", rc, uv_strerror(rc)); return rc; }

  rc = uv_tcp_keepalive( (uv_tcp_t*) &node->socket, 1, 180);  /* 180 seconds = 3 minutes */
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "worker thread: could not set TCP keep alive: (%d) %s", rc, uv_strerror(rc)); return rc; }

  /* set the state as connected */
  node->conn_state = CONN_STATE_CONNECTED;

  return SQLITE_OK;

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

  send_broadcast_message(plugin, "whr?");

  // it must stop the timer:
  // -on successful connection
  // -when the event loop is closing

}

/****************************************************************************/

SQLITE_PRIVATE void check_peer_connection(plugin *plugin, char *ip_address, int port) {
  node *node;

  if( is_local_ip_address(ip_address) ) return;

  /* check if already connected to this peer */

  for( node=plugin->peers; node; node=node->next ){
    if( strcmp(node->host,ip_address)==0 ){  //! what if there are more than 1 node on the same device? check this later
      if( node->conn_state==CONN_STATE_CONNECTING || node->conn_state==CONN_STATE_CONNECTED ){
        SYNCTRACE("check_peer_connection: %s already connected\n", ip_address);
        return;
      }else{
        SYNCTRACE("check_peer_connection: reconnecting to %s\n", ip_address);
        goto loc_reconnect;
      }
    }
  }

  /* connect to this peer */

  SYNCTRACE("check_peer_connection: connecting to %s\n", ip_address);

  node = new_node(plugin->loop);
  if( !node ) return;

  strcpy(node->host, ip_address);
  node->port = port;

loc_reconnect:

  node->conn_type = CONN_OUTGOING;
  connect_to_peer(node);

}

/****************************************************************************/

SQLITE_PRIVATE int is_local_ip_address(char *address){
  int count=0, i, ret=0;
  uv_interface_address_t *net_interface=NULL;  /* we cannot use the variable name 'interface' on MinGW */

  uv_interface_addresses(&net_interface, &count);
  for(i=0; i<count; i++){
    char local[17] = { 0 };
    int rc = uv_ip4_name(&net_interface[i].address.address4, local, 16);
    if( rc ){
      rc = uv_ip6_name(&net_interface[i].address.address6, local, 16);
    }
    SYNCTRACE("Local net_interface %d: %s\n", i, local);
    if( strcmp(address,local)==0 ){
      ret = 1;
    }
  }
  uv_free_interface_addresses(net_interface, count);
  return ret;

}

/****************************************************************************/

// option 1: send to the one that starts with 192 -- will not work on some networks
// option 2: send to 255.255.255.255 - it works
// option 3: send to all the interfaces, always
// option 4: send to all the interfaces the first time, the next ones use the address that returned responses.

SQLITE_PRIVATE int get_local_broadcast_address(char *address){

  //strcpy(address, "192.168.0.255");
  strcpy(address, "255.255.255.255");

  return SQLITE_OK;

#if 0
  int count=0, i, ret=0;
  uv_interface_address_t *net_interface=NULL;  /* we cannot use the variable name 'interface' on MinGW */

  uv_interface_addresses(&net_interface, &count);
  for(i=0; i<count; i++){
    char local[17] = { 0 };
    int rc = uv_ip4_name(&net_interface[i].address.address4, local, 16);
    if( rc ){
      rc = uv_ip6_name(&net_interface[i].address.address6, local, 16);
    }
    printf("Local net_interface %d: %s\n", i, local);
    if( strcmp(address,local)==0 ){
      ret = 1;
    }
  }
  uv_free_interface_addresses(net_interface, count);
  return ret;
#endif
}

/****************************************************************************/

SQLITE_PRIVATE int get_sockaddr_port(const struct sockaddr *sa) {
  if( sa->sa_family==AF_INET ){
    return ntohs(((struct sockaddr_in*)sa)->sin_port);
  }
  return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

/****************************************************************************/

SQLITE_PRIVATE void on_udp_send(uv_udp_send_t *req, int status) {
  if (status) {
    fprintf(stderr, "Send error %s\n", uv_strerror(status));
  }
  sqlite3_free(req);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void clear_leader_votes(plugin *plugin) {

  while( plugin->leader_votes ){
    struct leader_votes *next = plugin->leader_votes->next;
    sqlite3_free(plugin->leader_votes);
    plugin->leader_votes = next;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_election_period_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;

  plugin->in_election = FALSE;
  clear_leader_votes(plugin);

}

/****************************************************************************/

//! leader election: full nodes must have the preference, between those who have the last state

SQLITE_PRIVATE void new_leader_election(plugin *plugin) {

  SYNCTRACE("new_leader_election\n");

  if( plugin->in_election ) return;
  plugin->in_election = TRUE;

  plugin->leader_node = NULL;
  plugin->is_leader = FALSE;

  //! what if a (s)election is already taking place?
  clear_leader_votes(plugin);

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 2000, 0);
  uv_timer_start(&plugin->election_end_timer, on_election_period_timeout, 3000, 0);

  if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING || plugin->sync_down_state==DB_STATE_IN_SYNC ){
    plugin->sync_down_state = DB_STATE_UNKNOWN;
  }
  if( plugin->sync_up_state==DB_STATE_SYNCHRONIZING || plugin->sync_up_state==DB_STATE_IN_SYNC ){
    plugin->sync_up_state = DB_STATE_UNKNOWN;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void start_leader_election(plugin *plugin) {

  send_broadcast_message(plugin, "election");

}

/****************************************************************************/

SQLITE_PRIVATE void on_leader_check_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;
  struct leader_votes *votes;
  struct node *node;
  int biggest=0, total=0;

  SYNCTRACE("on_leader_check_timeout\n");

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->count > biggest ) biggest = votes->count;
    total += votes->count;
  }

  if( total<2 ){
    /* we need at least 3 nodes for a leader selection */
    SYNCTRACE("on_leader_check_timeout: no sufficient votes (total=%d)\n", total);
    goto loc_exit;
  }

  /* is more than one with this amount of votes? */
  total = 0;
  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->count == biggest ) total++;
  }
  if( total>1 ){
    SYNCTRACE("on_leader_check_timeout: no consensus on the current leader\n");
    //start_leader_election(plugin);  -- it can start a new election within a timeout
    goto loc_exit;
  }

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->count == biggest ) break;
  }

  if( votes->id==0 ){
    /* no current leader. start a election on all nodes including this one */
    SYNCTRACE("on_leader_check_timeout: no current leader\n");
    if( !plugin->in_election ){
      start_leader_election(plugin);
    }
    goto loc_exit;
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->id==votes->id ){
      plugin->leader_node = node;
      break;
    }
  }

  if( plugin->leader_node ){
    SYNCTRACE("on_leader_check_timeout: the current leader is at %s with id %d\n",
              plugin->leader_node->host, plugin->leader_node->id);

    start_downstream_db_sync(plugin);

  }else if( plugin->node_id==votes->id ){
    SYNCTRACE("on_leader_check_timeout: this node is the current leader\n");
    plugin->is_leader = TRUE;

    update_known_nodes(plugin);

    //leader_node_process_local_transactions(plugin);

  }else{
    SYNCTRACE("on_leader_check_timeout: not connected to the current leader\n");

    //check_peer_connection(ip_address, port);

    // maybe this is not required, as it is checking the connection to each
    // node that answers the broadcast request.

    send_broadcast_message(plugin, "whr?");

    if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING || plugin->sync_down_state==DB_STATE_IN_SYNC ){
      plugin->sync_down_state = DB_STATE_UNKNOWN;
    }
    if( plugin->sync_up_state==DB_STATE_SYNCHRONIZING || plugin->sync_up_state==DB_STATE_IN_SYNC ){
      plugin->sync_up_state = DB_STATE_UNKNOWN;
    }

    //plugin->leader_node = ...;  //! how to define it later?

  }

  /* activate a timer to retry the synchronization if it fails */
  SYNCTRACE("starting the process local transactions timer\n");
  uv_timer_start(&plugin->process_transactions_timer, process_transactions_timer_cb, 500, 500);

loc_exit:
  clear_leader_votes(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void check_current_leader(plugin *plugin) {

  plugin->leader_node = NULL;

  clear_leader_votes(plugin);

  send_broadcast_message(plugin, "leader?");

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 2000, 0);

}

/****************************************************************************/

/*
** calculation: last_block_height xor node->id => biggest
** not including the current leader
*/
SQLITE_PRIVATE int calculate_new_leader(plugin *plugin){
  aergolite *this_node = plugin->this_node;
  node *node;
  uint64 number, biggest=0, num_blocks, max_blocks=0;

  SYNCTRACE("calculate_new_leader\n");

  if( plugin->current_block ){
    num_blocks = plugin->current_block->height;
  }else{
    num_blocks = 0;
  }

  /* check the highest number of transactions */
  max_blocks = num_blocks;
  for( node=plugin->peers; node; node=node->next ){
    if( node->num_blocks>max_blocks ) max_blocks = node->num_blocks;
  }

  SYNCTRACE("calculate_new_leader max_blocks=%d\n", max_blocks);

  SYNCTRACE("calculate_new_leader node_id=%d\n", plugin->node_id);
  if( num_blocks==max_blocks ){
    number = max_blocks ^ (uint64)plugin->node_id;
    if( number>biggest ) biggest = number;
  }

  for( node=plugin->peers; node; node=node->next ){
    SYNCTRACE("calculate_new_leader node_id=%d num_blocks=%" INT64_FORMAT "\n", node->id, node->num_blocks);
    if( node->num_blocks==max_blocks && node!=plugin->last_leader ){
      number = max_blocks ^ (uint64)node->id;
      if( number>biggest ) biggest = number;
    }
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->num_blocks==max_blocks && node!=plugin->last_leader ){
      number = max_blocks ^ (uint64)node->id;
      if( number==biggest ) return node->id;
    }
  }

  return plugin->node_id;
}

/****************************************************************************/

SQLITE_PRIVATE void broadcast_new_leader(plugin *plugin){
  char message[32];
  int leader_id;

  /* xxx */

  leader_id = calculate_new_leader(plugin);

  SYNCTRACE("broadcast_new_leader id=%d\n", leader_id);

  sprintf(message, "leader:%d", leader_id);

  send_broadcast_message(plugin, message);

}

/****************************************************************************/

SQLITE_PRIVATE void election_info_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;

  broadcast_new_leader(plugin);

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void on_udp_message(uv_udp_t *socket, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
  uv_loop_t *loop = socket->loop;
  plugin *plugin = (struct plugin *) loop->data;
  aergolite *this_node = plugin->this_node;
  char sender[17] = { 0 };

  if( nread<0 ){
    sqlite3_log(SQLITE_ERROR, "Read error %s\n", uv_err_name(nread));
    uv_close2((uv_handle_t*) socket, worker_thread_on_close);
    goto loc_exit;
  }else if( nread==0 ){
    goto loc_exit;
  }

  uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
  SYNCTRACE("on_udp_message from %s: %.*s len=%u\n", sender, (int)nread, buf->base, nread);

  if( strcmp(buf->base,"whr?")==0 ){  /* a broadcast message to find peers */
    uv_udp_send_t *send_req;
    uv_buf_t response = { .base="here", .len=5 };

    if( is_local_ip_address(sender) ){
      goto loc_exit;
    }

    /* send a response message */

    SYNCTRACE("on_udp_message: send a response message to %s\n", sender);

    send_req = sqlite3_malloc(sizeof(uv_udp_send_t));
    if (!send_req) goto loc_exit;

    uv_udp_send(send_req, socket, &response, 1, addr, on_udp_send);


  }else if( strcmp(buf->base,"here")==0 ){  /* a response message informing a peer location */

    check_peer_connection(plugin, sender, get_sockaddr_port(addr));

    if( plugin->reconnect_timer_enabled ){
      /* disable the reconnection timer */
      uv_timer_stop(&plugin->reconnect_timer);
      plugin->reconnect_timer_enabled = 0;
      /* enable the after connections timer */
      uv_timer_start(&plugin->after_connections_timer, after_connections_timer_cb, 500, 0);
    }


  }else if( strcmp(buf->base,"leader?")==0 ){  /* a broadcast message requesting the current leader */
    uv_udp_send_t *send_req;
    uv_buf_t response;
    int leader_id;

    if( is_local_ip_address(sender) ){
      goto loc_exit;
    }

    /* send a response message */

    SYNCTRACE("on_udp_message: send a response message to %s\n", sender);

    if( plugin->is_leader ){
      leader_id = plugin->node_id;
    }else if( plugin->leader_node ){
      leader_id = plugin->leader_node->id;
    }else{
      leader_id = 0;
    }

    //sprintf(leader_buf, "leader:%d", leader_id);
    //response.base = leader_buf;
    //response.len = strlen(leader_buf) + 1;

    send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + 32);  /* allocates additional space for the response string */
    if (!send_req) goto loc_exit;
    response.base = (char*)send_req + sizeof(uv_udp_send_t);

    sprintf(response.base, "leader:%d", leader_id);
    response.len = strlen(response.base) + 1;

    uv_udp_send(send_req, socket, &response, 1, addr, on_udp_send);


  }else if( strcmp(buf->base,"election")==0 ){  /* a broadcast message requesting a leader election */

    if( !plugin->in_election ){

      if( plugin->leader_node ){
        /* check if the current leader is still alive */
        send_udp_message(plugin, plugin->leader_node->host, "ping");
        /* we cannot send new txns to the current leader until the election is done */
        plugin->last_leader = plugin->leader_node;
        plugin->leader_node = NULL;
      }else{
        plugin->last_leader = NULL;
      }

      new_leader_election(plugin);
      uv_timer_start(&plugin->election_info_timer, election_info_timeout, 1000, 0);

      {
        //send_broadcast_messagef(plugin, "num_blocks:%d:%d", num_blocks, plugin->node_id);
        char message[64];
        int num_blocks = plugin->current_block ? plugin->current_block->height : 0;
        SYNCTRACE("this node's last block height: %d\n", num_blocks);
        sprintf(message, "num_blocks:%d:%d", num_blocks, plugin->node_id);
        send_broadcast_message(plugin, message);
      }

    }


  }else if( strcmp(buf->base,"pong")==0 ){  /* a response message from the current leader */

    /* xxx */

    if( plugin->leader_node==NULL && plugin->last_leader ){   //! what if the leader is not connected via TCP?
      char message[32];
      SYNCTRACE("the current leader answered\n");
      sprintf(message, "leader:%d", plugin->last_leader->id);
      send_broadcast_message(plugin, message);
      uv_timer_stop(&plugin->election_info_timer);
    }


  }else if( strncmp(buf->base,"num_blocks:",11)==0 ){  /* a response message informing how many txns on the node's blockchain */

    node *node;
    int node_id=0;
    int num_blocks=0;
    char *pid, *pnum;

    check_peer_connection(plugin, sender, get_sockaddr_port(addr));

    pnum = buf->base + 11;
    pid = stripchr(pnum, ':');
    num_blocks = atoi(pnum);
    node_id = atoi(pid);

    SYNCTRACE("node %d last block height: %d\n", node_id, num_blocks);

    for( node=plugin->peers; node; node=node->next ){
      if( node->id==node_id ){
        node->num_blocks = num_blocks;
      }
    }


  }else if( strncmp(buf->base,"leader:",7)==0 ){  /* a response message informing the peer leader */

    struct leader_votes *votes;
    int node_id=0;

    check_peer_connection(plugin, sender, get_sockaddr_port(addr));

    node_id = atoi(buf->base+7);

    for( votes=plugin->leader_votes; votes; votes=votes->next ){
      if( votes->id==node_id ){
        votes->count++;
        break;
      }
    }

    if( !votes ){  /* no item allocated for the given node id */
      votes = sqlite3_malloc(sizeof(struct leader_votes));
      if( !votes ) goto loc_exit;
      votes->id = node_id;
      votes->count = 1;
      /* add it to the list */
      votes->next = plugin->leader_votes;
      plugin->leader_votes = votes;
    }

    /* stop the election timer if the number of votes for a single node reaches the majority */
    if( plugin->total_known_nodes>1 && votes->count >= majority(plugin->total_known_nodes) ){
      uv_timer_stop(&plugin->leader_check_timer);
      on_leader_check_timeout(&plugin->leader_check_timer);
    }


  }else if( nread==17 && strncmp(buf->base,"blockchain_status",17)==0 ){  /* a broadcast message requesting status info */

    char *status = aergolite_get_blockchain_status(this_node);
    if( status ){
      send_udp_message(plugin, sender, status);
      sqlite3_free(status);
    }


  }else if( nread>=15 && strncmp(buf->base,"protocol_status",15)==0 ){  /* a broadcast message requesting status info */

    char *remaining = buf->base + 15;
    char *status;
    BOOL extended = 0;

    if( strncmp(remaining,"(1)",3)==0 ){  /* extended status info */
      extended = 1;
    }

    status = get_protocol_status(plugin, extended);
    if( status ){
      send_udp_message(plugin, sender, status);
      sqlite3_free(status);
    }


  }else if( nread==9 && strncmp(buf->base,"node_info",9)==0 ){  /* a broadcast message requesting node info */

    char *info = aergolite_get_node_info(this_node);

    if( info ){
      send_udp_message(plugin, sender, info);
      sqlite3_free(info);
    }else{
      send_udp_message(plugin, sender, "");
    }

  }

loc_exit:

  sqlite3_free(buf->base);

}

/****************************************************************************/

SQLITE_PRIVATE int send_broadcast_message(plugin *plugin, char *message) {
  struct sockaddr_in dest_addr;
  uv_udp_send_t *send_req;
  char *address;
  int port, rc = SQLITE_OK;
  uv_buf_t buffer;

  //buffer.base = message;
  //buffer.len = strlen(message) + 1;

  if( !plugin->bind ) return SQLITE_OK;

  address = plugin->bind->host;  // ->broadcast_address
  port = plugin->bind->port;     // ->broadcast_port

  SYNCTRACE("send_broadcast_message [%s:%d]: %s\n", address, port, message);

  if( uv_ip4_addr(address, port, (struct sockaddr_in *) &dest_addr)!=0 ){
    if( uv_ip6_addr(address, port, (struct sockaddr_in6 *) &dest_addr)!=0 ){
      sqlite3_log(SQLITE_ERROR, "send_broadcast_message: invalid address [%s:%d]", address, port);
      return SQLITE_ERROR;
    }
  }

  buffer.len = strlen(message) + 1;
  send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + buffer.len);  /* allocates additional space for the response string */
  if (!send_req) return SQLITE_NOMEM;
  buffer.base = (char*)send_req + sizeof(uv_udp_send_t);
  strcpy(buffer.base, message);

  uv_udp_set_broadcast(plugin->udp_sock, 1);
  rc = uv_udp_send(send_req, plugin->udp_sock, &buffer, 1, (const struct sockaddr *)&dest_addr, on_udp_send);
  if( rc ) rc = SQLITE_ERROR;
  return rc;

}

/****************************************************************************/

SQLITE_PRIVATE int send_udp_message(plugin *plugin, char *address, char *message) {
  struct sockaddr_in dest_addr;
  uv_udp_send_t *send_req;
  int port, rc = SQLITE_OK;
  uv_buf_t buffer;

  if( !plugin->bind ) return SQLITE_OK;

  port = plugin->bind->port;     // ->broadcast_port

  SYNCTRACE("send_udp_message [%s:%d]: %s\n", address, port, message);

  if( uv_ip4_addr(address, port, (struct sockaddr_in *) &dest_addr)!=0 ){
    if( uv_ip6_addr(address, port, (struct sockaddr_in6 *) &dest_addr)!=0 ){
      sqlite3_log(SQLITE_ERROR, "send_udp_message: invalid address [%s:%d]", address, port);
      return SQLITE_ERROR;
    }
  }

  buffer.len = strlen(message) + 1;
  send_req = sqlite3_malloc(sizeof(uv_udp_send_t) + buffer.len);  /* allocates additional space for the response string */
  if (!send_req) return SQLITE_NOMEM;
  buffer.base = (char*)send_req + sizeof(uv_udp_send_t);
  strcpy(buffer.base, message);

  rc = uv_udp_send(send_req, plugin->udp_sock, &buffer, 1, (const struct sockaddr *)&dest_addr, on_udp_send);
  if( rc ) rc = SQLITE_ERROR;
  return rc;

}

/****************************************************************************/
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
    SYNCTRACE("peer connections - binding to address: %s:%d\n", "0.0.0.0", address->port);
    if( uv_ip4_addr("0.0.0.0", address->port, (struct sockaddr_in *) &addr)!=0 ){
      if( uv_ip6_addr("0.0.0.0", address->port, (struct sockaddr_in6 *) &addr)!=0 ){
        sqlite3_log(SQLITE_ERROR, "worker thread: invalid address [%s:%d]", "0.0.0.0", address->port);
        goto loc_failed;
      }
    }

    server = sqlite3_malloc(sizeof(uv_tcp_t));
    if (!server) goto loc_no_memory;
    uv_tcp_init(&loop, server);
    uv_tcp_bind(server, (const struct sockaddr*) &addr, 0);
    if ((rc = uv_listen((uv_stream_t*) server, DEFAULT_BACKLOG, worker_thread_on_incoming_tcp_connection))) {
      sqlite3_log(SQLITE_ERROR, "worker thread: listen on TCP socket [%s:%d] failed: (%d) %s", address->host, address->port, rc, uv_strerror(rc));
      //sqlite3_free(server);
      goto loc_failed;
    }

    plugin->udp_sock = sqlite3_malloc_zero(sizeof(uv_udp_t));
    if (!plugin->udp_sock) goto loc_no_memory;
    uv_udp_init(&loop, plugin->udp_sock);
    uv_udp_bind(plugin->udp_sock, (const struct sockaddr*) &addr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(plugin->udp_sock, alloc_buffer, on_udp_message);

    break;  //! by now only one address is supported
  }


  /* send broadcast message to find the peers */
  /* on a separate loop because on the future it can hold broadcast addresses, that can
  ** differ from the bind addresses. eg:
  **  "discovery=192.168.1.255:1234,10.0.34.255:1234"
  **  the same bind address (0.0.0.0:1234) but 2 broadcast addresses.
  */
  for (address = plugin->bind; address; address = address->next) {

    SYNCTRACE("peer connections - sending discovery packets to address: %s:%d\n", address->host, address->port);

    send_broadcast_message(plugin, "whr?");

    break;  //! by now only one address is supported
  }



  /* initialize the timers */
  uv_timer_init(&loop, &plugin->after_connections_timer);
  uv_timer_init(&loop, &plugin->leader_check_timer);
  uv_timer_init(&loop, &plugin->election_info_timer);
  uv_timer_init(&loop, &plugin->election_end_timer);
  uv_timer_init(&loop, &plugin->reconnect_timer);

  uv_timer_init(&loop, &plugin->process_transactions_timer);
  uv_timer_init(&loop, &plugin->new_block_timer);

  uv_timer_init(&loop, &plugin->aergolite_core_timer);


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

SQLITE_API void plugin_end(void *arg){
  plugin *plugin = (struct plugin *) arg;

  /* close the worker thread with all the connections */
  close_worker_thread(plugin);

  while( plugin->bind ){
    struct tcp_address *next = plugin->bind->next;
    sqlite3_free(plugin->bind);
    plugin->bind = next;
  }

  sqlite3_free(plugin);

}

/****************************************************************************/
/****************************************************************************/

#define DEFAULT_RECONNECT_INTERVAL   3000  // milliseconds = 3 seconds

SQLITE_PRIVATE struct tcp_address * parse_tcp_address(char *address, int common_reconnect_interval) {
  struct tcp_address *first=0, *prev=0, *addr=0;

  if( !address ) return NULL;

  address = sqlite3_strdup(address);

  while (address) {
    char *host, *zport, *next;

    next = stripchr(address, ',');

    if (strncmp(address, "tcp://", 6) != 0) {
      sqlite3_log(SQLITE_ERROR, "the protocol is not supported: %s", address);
      goto loc_failed;
    }

    addr = sqlite3_malloc(sizeof(struct tcp_address));
    if (!addr) {
      sqlite3_log(SQLITE_NOMEM, "out of memory");
      goto loc_failed;
    }
    memset(addr, 0, sizeof(struct tcp_address));

    if (!first) first = addr;
    if (prev) prev->next = addr;
    prev = addr;

    host = address + 6;  /* strlen("tcp://"); */

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
    strcpy(addr->host, host);

    if (common_reconnect_interval > 0) {
      addr->reconnect_interval = common_reconnect_interval;
    } else {
      addr->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
    }

    address = next;
  }

  sqlite3_free(address);
  return first;
loc_failed:
  addr = first;
  while( addr ){
    struct tcp_address *next = addr->next;
    sqlite3_free(addr);
    addr = next;
  }
  sqlite3_free(address);
  return (struct tcp_address *)-1;
}

/****************************************************************************/

SQLITE_PRIVATE struct tcp_address * parse_discovery_address(char *address, int common_reconnect_interval) {
  struct tcp_address *first=0, *prev=0, *addr=0;

  if( !address ) return NULL;

  address = sqlite3_strdup(address);

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
      get_local_broadcast_address(addr->host);
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

  sqlite3_free(address);  //! it will fail with 2 addresses
  return first;
loc_failed:
  addr = first;
  while( addr ){
    struct tcp_address *next = addr->next;
    sqlite3_free(addr);
    addr = next;
  }
  sqlite3_free(address);
  return (struct tcp_address *)-1;
}

/****************************************************************************/

void * plugin_init(aergolite *this_node, char *uri) {
  struct tcp_address *addr;
  plugin *plugin;
  char *discovery;
  int64 random_no;

  SYNCTRACE("initializing a new instance of mini-raft plugin\n");

  /* allocate a new plugin object */

  plugin = sqlite3_malloc_zero(sizeof(struct plugin));
  if( !plugin ) return NULL;  //SQLITE_NOMEM;

  plugin->this_node = this_node;

  plugin->node_id = aergolite_get_node_id(this_node);


  plugin->is_leader = FALSE;


  /* parse the node discovery parameter */

  discovery = (char*) sqlite3_uri_parameter(uri, "discovery");
  /*
  ** if no node discovery is supplied, the database can only be used locally
  ** without connections to other nodes.
  */
  if( discovery ){
    plugin->bind = parse_discovery_address(discovery, 0);
    if( plugin->bind==(struct tcp_address *)-1 ) goto loc_failed;
    for (addr = plugin->bind; addr; addr = addr->next) {
      SYNCTRACE("  bind address: 0.0.0.0:%d \n", addr->port);
      SYNCTRACE("  discovery address: %s:%d \n", addr->host, addr->port);
    }
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
    get_protocol_status
  );

  return rc;
}
