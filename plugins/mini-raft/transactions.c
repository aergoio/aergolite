
/****************************************************************************/

SQLITE_PRIVATE void on_local_transaction_sent(send_message_t *req, int status) {

  if (status < 0) {
    //plugin->sync_up_state = DB_STATE_UNKNOWN;
    sqlite3_log(status, "sending local transaction: %s\n", uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE int send_local_transaction_data(plugin *plugin, int64 nonce, binn *log) {
  aergolite *this_node = plugin->this_node;
  binn *map=NULL;
  BOOL ret;

  SYNCTRACE("send_local_transaction_data - nonce=%" INT64_FORMAT "\n", nonce);

  plugin->sync_up_state = DB_STATE_SYNCHRONIZING;

  map = binn_map();
  if( !map ) goto loc_failed;

  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_INSERT_TRANSACTION);
  binn_map_set_int32(map, PLUGIN_NODE_ID, plugin->node_id);
  binn_map_set_int64(map, PLUGIN_NONCE, nonce);
  binn_map_set_list(map, PLUGIN_SQL_CMDS, log);

  ret = send_peer_message(plugin->leader_node, map, on_local_transaction_sent);
  if( ret==FALSE ) goto loc_failed;

  binn_free(map);

  return SQLITE_OK;

loc_failed:

  sqlite3_log(1, "send_local_transaction_data failed");

  plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;

  if( map ) binn_free(map);

  return SQLITE_ERROR;

}

/****************************************************************************/

SQLITE_PRIVATE void send_local_transactions(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  int64 nonce;
  binn *log = NULL;
  int rc;

  SYNCTRACE("send_local_transactions\n");

  nonce = 0; /* we don't know what is the nonce of the first txn in the queue */

  while( 1 ){
    rc = aergolite_get_local_transaction(this_node, &nonce, &log);

    if( rc==SQLITE_EMPTY || rc==SQLITE_NOTFOUND ){
      SYNCTRACE("send_local_transactions - no more local transactions - IN SYNC\n");
      plugin->sync_up_state = DB_STATE_IN_SYNC;
      return;
    } else if( rc!=SQLITE_OK || nonce==0 || log==0 ){
      sqlite3_log(rc, "send_local_transactions FAILED - nonce=%" INT64_FORMAT, nonce);
      plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
      return;
    }

    if( !plugin->leader_node ){
      SYNCTRACE("send_local_transactions - no leader node\n");
      plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
      aergolite_free_transaction(log);
      /* if not already searching for a leader node, start it now */
      check_current_leader(plugin);
      return;
    }

    rc = send_local_transaction_data(plugin, nonce, log);
    aergolite_free_transaction(log);
    if( rc ) return;

    nonce++;
  }

}

/****************************************************************************/
/****************************************************************************/

#if 0

SQLITE_PRIVATE void on_transaction_exists(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int64 tid;

  tid = binn_map_int64(msg, PLUGIN_TID);

  SYNCTRACE("on_transaction_exists - tid=%" INT64_FORMAT "\n", tid);

}

/****************************************************************************/

SQLITE_PRIVATE void on_transaction_failed(plugin *plugin, int64 tid, int rc) {
  aergolite *this_node = plugin->this_node;

  if( rc==SQLITE_BUSY ){  /* temporary failure */
    SYNCTRACE("on_transaction_failed - TEMPORARY FAILURE - tid=%" INT64_FORMAT "\n", tid);
    /* retry the command */
  } else {  /* definitive failure */
    struct transaction *txn;
    /* the command is invalid and it cannot be included in the list */
    SYNCTRACE("on_transaction_failed - DEFINITIVE FAILURE - tid=%" INT64_FORMAT " rc=%d\n", tid, rc);
    /* search for the transaction in the mempool */
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->id==tid ) break;
    }
    if( txn ){
      /* remove the transaction from the mempool */
      discard_mempool_transaction(plugin, txn);
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_transaction_failed_msg(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int64 tid;
  int rc;

  tid = binn_map_int64(msg, PLUGIN_TID);
  rc = binn_map_int32(msg, PLUGIN_ERROR);

  on_transaction_failed(plugin, tid, rc);

}

#endif

/****************************************************************************/

/*
** Used by the leader.
*/

// retrieve it from the mempool or, if not found, from the db (if full node)

SQLITE_PRIVATE void on_get_transaction(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  struct transaction *txn, tx1={0};
  int64 tid;
  binn *map;
  int rc;

  tid = binn_map_int64(msg, PLUGIN_TID);

  SYNCTRACE("on_get_transaction - request from node %d - tid=%" INT64_FORMAT "\n", node->id, tid);

  map = binn_map();
  if( !map ) return;  //goto loc_failed;

  /* check if the transaction is in the mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->id==tid ) break;
  }

  if( txn ){
    rc = SQLITE_OK;
  }else{
    /* load it from the database */
    //txn = &tx1;
    //rc = aergolite_get_transaction(this_node, tid, &txn->node_id, &txn->nonce, &txn->log);
    rc = SQLITE_NOTFOUND;
  }

  switch( rc ){
  case SQLITE_NOTFOUND:
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_TXN_NOTFOUND);
    break;
  case SQLITE_OK:
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUESTED_TRANSACTION);
    //binn_map_set_int64(map, PLUGIN_TID, tid);
    binn_map_set_int32(map, PLUGIN_NODE_ID, txn->node_id);
    binn_map_set_int64(map, PLUGIN_NONCE, txn->nonce);
    binn_map_set_list (map, PLUGIN_SQL_CMDS, txn->log);
    if( txn==&tx1 ) sqlite3_free(txn->log);
    break;
  default:
    sqlite3_log(rc, "on_get_transaction: aergolite_get_transaction failed");
    goto loc_exit;
  }

  send_peer_message(node, map, NULL);  // on_local_transaction_sent);

loc_exit:

  if (map) binn_free(map);

}

/****************************************************************************/

SQLITE_PRIVATE void send_mempool_transactions(plugin *plugin, node *node) {
  struct transaction *txn;

  SYNCTRACE("send_mempool_transactions - to node %d\n", node->id);

  for( txn=plugin->mempool; txn; txn=txn->next ){
    binn *map = binn_map();
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_TRANSACTION);
    binn_map_set_int32(map, PLUGIN_NODE_ID, txn->node_id);
    binn_map_set_int64(map, PLUGIN_NONCE, txn->nonce);
    binn_map_set_list (map, PLUGIN_SQL_CMDS, txn->log);
    send_peer_message(node, map, NULL);
    binn_free(map);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_get_mempool_transactions(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_get_mempool_transactions - request from node %d\n", node->id);

  send_mempool_transactions(plugin, node);

}

/****************************************************************************/

/*
** Used by the leader.
*/
SQLITE_PRIVATE int broadcast_transaction(plugin *plugin, struct transaction *txn) {
  struct node *node;
  binn *map;

  SYNCTRACE("broadcast_transaction"
            " node=%d nonce=%" INT64_FORMAT " sql_count=%d\n",
            txn->node_id, txn->nonce, binn_count(txn->log)-2 );

  /* signal other peers that there is a new transaction */
  map = binn_map();
  if( !map ) return SQLITE_BUSY;  /* flag to retry the command later */

  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_TRANSACTION);
  binn_map_set_int32(map, PLUGIN_NODE_ID, txn->node_id);
  binn_map_set_int64(map, PLUGIN_NONCE, txn->nonce);
  binn_map_set_list (map, PLUGIN_SQL_CMDS, txn->log);

  for( node=plugin->peers; node; node=node->next ){
//    if( node->id!=txn->node_id ){   -- should it notify the same node?
      send_peer_message(node, map, NULL);
//    }
  }

  binn_free(map);

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE int update_last_nonce_array(
  plugin *plugin,
  int node_id,
  int64 nonce
){
  struct node_nonce *item, new_item = {0};
  int pos;

  SYNCTRACE("update_last_nonce_array node_id=%d nonce=%" INT64_FORMAT
            "\n", node_id, nonce);

  if( plugin->nonces==NULL ){
    plugin->nonces = new_array(4, sizeof(struct node_nonce));
    if( plugin->nonces==NULL ){
      SYNCTRACE("update_last_nonce_array FAILED: %s\n", "out of memory");
      return SQLITE_NOMEM;
    }
  }else{
    int i, count = array_count(plugin->nonces);
    for( i=0; i<count; i++ ){
      item = array_get(plugin->nonces, i);
      if( item->node_id==node_id ){
        /* found. just update the value */
        item->last_nonce = nonce;
        return SQLITE_OK;
      }
    }
  }

  /* not found. add a new item to the array */
  new_item.node_id = node_id;
  new_item.last_nonce = nonce;
  pos = array_append(&plugin->nonces, &new_item);
  if( pos<0 ){
    SYNCTRACE("update_last_nonce_array FAILED: %s\n", "adding new item");
    return SQLITE_NOMEM;
  }

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE void build_last_nonce_array_cb(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  int64 last_nonce
){
  struct plugin *plugin = (struct plugin *) arg;
  struct transaction *txn;

  /* check if there are transactions from this node on the local mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->node_id==node_id && txn->block_height==0 ){
      /* add the node's last_nonce to the array */
      update_last_nonce_array(plugin, node_id, last_nonce);
      break;
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE int build_last_nonce_array(plugin *plugin){
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("build_last_nonce_array\n");

  if( plugin->nonces ){
    array_free(&plugin->nonces);
  }

  return aergolite_iterate_allowed_nodes(this_node, build_last_nonce_array_cb, plugin);
}

/****************************************************************************/

SQLITE_PRIVATE void check_mempool_transaction_cb(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  int64 last_nonce
){
  struct plugin *plugin = (struct plugin *) arg;
  struct transaction *txn;

loc_repeat:

  /* remove the old transactions from the local mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->node_id==node_id && txn->nonce<=last_nonce && txn->block_height==0 ){
      discard_mempool_transaction(plugin, txn);
      goto loc_repeat;
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE int check_mempool_transactions(plugin *plugin){
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("check_mempool_transactions\n");

  return aergolite_iterate_allowed_nodes(this_node, check_mempool_transaction_cb, plugin);
}

/****************************************************************************/

SQLITE_PRIVATE int store_transaction_on_mempool(
  plugin *plugin, int node_id, int64 nonce, void *log, struct transaction **ptxn
){
  struct transaction *txn;
  int64 tid;
  int rc;

  SYNCTRACE("store_transaction_on_mempool node_id=%d nonce=%"
            INT64_FORMAT "\n", node_id, nonce);

  tid = aergolite_get_transaction_id(node_id, nonce);

  /* check if the transaction is already in the local mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->id==tid ){
      SYNCTRACE("store_transaction_on_mempool - transaction already present\n");
      if( ptxn ) *ptxn = txn;
      return SQLITE_EXISTS;
    }
  }

  /* check if this transaction is valid */
  rc = aergolite_verify_transaction(plugin->this_node, node_id, log);
  if( rc!=SQLITE_OK ) return rc;

  /* allocate a new transaction object */
  txn = sqlite3_malloc_zero(sizeof(struct transaction));
  if( !txn ) return SQLITE_NOMEM;

  /* add it to the list */
  llist_add(&plugin->mempool, txn);

  /* store the transaction data */
  txn->node_id = node_id;
  txn->nonce = nonce;
  txn->id = tid;
  txn->log = sqlite3_memdup(binn_ptr(log), binn_size(log));
  //! it could create a copy here using only the SQL commands and removing not needed data. or maybe use netstring...
  //txn->data = xxx(log);    //! or maybe let the consensus protocol decide what to store here...
  //! or the core could supply the txn already without the metadata
  // but: probably the txn will already be signed in the WAL file. in this case it cannot be changed here

  if( ptxn ) *ptxn = txn;
  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE void discard_mempool_transaction(plugin *plugin, struct transaction *txn){

  SYNCTRACE("discard_mempool_transaction\n");

  /* remove the transaction from the mempool */
  if( txn ){
    llist_remove(&plugin->mempool, txn);
    if( txn->log ) sqlite3_free(txn->log);
    sqlite3_free(txn);
  }

}

/****************************************************************************/

/*
** Used by the leader.
** -verify the transaction  ??
** -store the transaction in the local mempool,
** -if the timer to generate a new block is not started, start it now, and
** -broadcast the transaction to all the peers.
*/
SQLITE_PRIVATE int process_new_transaction(plugin *plugin, int node_id, int64 nonce, void *log) {
  struct transaction *txn;
  int rc;

  SYNCTRACE("process_new_transaction - node=%d nonce=%" INT64_FORMAT " sql_count=%d\n",
            node_id, nonce, binn_count(log)-2 );

  /* store the transaction in the local mempool */
  rc = store_transaction_on_mempool(plugin, node_id, nonce, log, &txn);
  if( rc==SQLITE_EXISTS ) return SQLITE_OK;
  if( rc ) return rc;

  /* start the timer to generate a new block */
  start_new_block_timer(plugin);

  /* broadcast the transaction to all the peers */
  rc = broadcast_transaction(plugin, txn);

  return rc;
}

/****************************************************************************/

/*
** The leader node received a new transaction from a follower node
*/
SQLITE_PRIVATE void on_insert_transaction(node *source_node, void *msg, int size) {
  plugin *plugin = source_node->plugin;
  aergolite *this_node = source_node->this_node;
  int64 nonce;
  BOOL tr_exists;
  void *log=0;
  int  rc;

  nonce = binn_map_int64(msg, PLUGIN_NONCE);
  log = binn_map_list(msg, PLUGIN_SQL_CMDS);

  SYNCTRACE("on_insert_transaction - from node %d - nonce: %" INT64_FORMAT
            " sql count: %d\n", source_node->id, nonce, binn_count(log)-2 );


#if 0

  //! it must call a fn to check the nonce for this node


  if( aergolite_check_transaction_in_blockchain(this_node,tid,&tr_exists)!=SQLITE_OK ){
    sqlite3_log(1, "check_transaction_in_blockchain failed");
    rc = SQLITE_BUSY;  /* to retry again */
    goto loc_failed;
  }

  if( tr_exists ){
    binn *map = binn_map();
    if (!map) return;
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_LOG_EXISTS);
    binn_map_set_int64(map, PLUGIN_NONCE, nonce);
    send_peer_message(source_node, map, NULL);
    binn_free(map);
    return;
  }

#endif


  rc = process_new_transaction(plugin, source_node->id, nonce, log);
  if( rc ) goto loc_failed;

  return;

loc_failed:

  if( rc==SQLITE_OK ) rc = SQLITE_BUSY;  /* to retry again */
  {
    binn *map = binn_map();
    if (!map) return;
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_TRANSACTION_FAILED);
    binn_map_set_int64(map, PLUGIN_NONCE, nonce);
    binn_map_set_int32(map, PLUGIN_ERROR, rc);
    send_peer_message(source_node, map, NULL);
    binn_free(map);
  }

}

/****************************************************************************/

/*
** A new transaction was received from the leader node
*/
SQLITE_PRIVATE int on_new_remote_transaction(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  struct transaction *txn;
  int node_id;
  int64 nonce;
  void *log;

  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);
  nonce   = binn_map_int64(msg, PLUGIN_NONCE);
  log     = binn_map_list (msg, PLUGIN_SQL_CMDS);

  SYNCTRACE("on_new_remote_transaction - node_id=%d nonce=%" INT64_FORMAT
            " sql_count=%d\n", node_id, nonce, binn_count(log)-2 );

  /* store the transaction in the local mempool */
  return store_transaction_on_mempool(plugin, node_id, nonce, log, NULL);
}

/****************************************************************************/

SQLITE_PRIVATE void leader_node_process_local_transactions(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  int64 nonce;
  binn *log=NULL;
  int   rc;

  SYNCTRACE("leader_node_process_local_transactions\n");

  nonce = 0;

  while( 1 ){
    rc = aergolite_get_local_transaction(this_node, &nonce, &log);

    if( rc==SQLITE_EMPTY || rc==SQLITE_NOTFOUND ){
      SYNCTRACE("leader_node_process_local_transactions - no more local transactions - IN SYNC\n");
      plugin->sync_up_state = DB_STATE_IN_SYNC;
      return;
    } else if( rc!=SQLITE_OK || nonce==0 || log==0 ){
      SYNCTRACE("--- leader_node_process_local_transactions FAILED - rc=%d nonce=%" INT64_FORMAT " log=%p\n", rc, nonce, log);
      plugin->sync_up_state = DB_STATE_UNKNOWN;
      goto loc_try_later;
    }

    // it must enter in a consensus with other nodes before executing a transaction.
    // this process is asynchronous, it must receive notifications from the other nodes.
    // it counts the number of received messages for the sent transaction.
    // then waits until it receives response messages from the majority of the peers (including itself)

    // so the nodes must store the total number of nodes (or the list of nodes) and they must
    // agree on this list - it can be on the blockchain!

    rc = process_new_transaction(plugin, plugin->node_id, nonce, log);
    aergolite_free_transaction(log);
    if( rc ) goto loc_try_later;

    nonce++;
  }


loc_try_later:

  plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;

  /* activate a timer to retry it again later */
//  SYNCTRACE("starting the process local transactions timer\n");
//  uv_timer_start(&plugin->process_transactions_timer, process_transactions_timer_cb, 100, 0);

}

/****************************************************************************/

SQLITE_PRIVATE void follower_node_on_local_transaction(plugin *plugin) {

  SYNCTRACE("follower_node_on_local_transaction\n");

  /* if this node is in sync, just send the new local transaction. otherwise start the sync process */

  if( plugin->sync_up_state!=DB_STATE_SYNCHRONIZING ){
    /* update the upstream state */
    plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
    /* check if already downloaded all txns */
    if( plugin->sync_down_state==DB_STATE_IN_SYNC ){
      /* send the new local transaction */
      send_local_transactions(plugin);
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE void leader_node_on_local_transaction(plugin *plugin) {

  SYNCTRACE("leader_node_on_local_transaction\n");

  plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;

  leader_node_process_local_transactions(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void db_sync_on_local_transaction(plugin *plugin) {

  SYNCTRACE("db_sync_on_local_transaction\n");

  if( plugin->is_leader ){
    leader_node_on_local_transaction(plugin);
  }else{
    follower_node_on_local_transaction(plugin);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_local_transaction(plugin *plugin) {

  SYNCTRACE("worker thread: on new local transaction\n");

  /* start the db sync if not yet started */
  db_sync_on_local_transaction(plugin);

}

/****************************************************************************/

/*
** This function is called on the main thread. It must send the notification
** to the worker thread and return as fast as possible.
*/
SQLITE_API void on_new_local_transaction(void *arg) {
  plugin *plugin = (struct plugin *) arg;

  SYNCTRACE("on_new_local_transaction\n");

  if( plugin->thread_active ){
    int rc, cmd = WORKER_THREAD_NEW_TRANSACTION;
    /* send command to the worker thread */
    SYNCTRACE("sending worker thread command: new local transaction\n");
    if( (rc=send_notification_to_worker(plugin, (char*)&cmd, sizeof(cmd))) < 0 ){
      SYNCTRACE("send_notification_to_worker failed: (%d) %s\n", rc, uv_strerror(rc));
    }
  }

}
