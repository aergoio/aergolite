
SQLITE_PRIVATE int process_new_transaction(
  plugin *plugin,
  int node_id,
  int64 nonce,
  void *log,
  struct transaction **ptxn
);

SQLITE_PRIVATE int broadcast_transaction(plugin *plugin, struct transaction *txn);

/****************************************************************************/

SQLITE_PRIVATE int txn_sql_count(void *log){
  binn_iter iter;
  binn value;
  int count = 0;

  binn_list_foreach(log, value){
    if( value.type==BINN_STRING ){
      count++;
    }
  }

  return count;
}

/****************************************************************************/

SQLITE_PRIVATE void send_local_transactions(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
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

    if( !plugin->peers ){
      SYNCTRACE("send_local_transactions - no connected nodes\n");
      goto loc_failed;
    }

    rc = process_new_transaction(plugin, plugin->node_id, nonce, log, &txn);

    if( rc==SQLITE_OK ){
      /* broadcast the transaction to all the peers */
      rc = broadcast_transaction(plugin, txn);
    }

    aergolite_free_transaction(log);
    if( rc ) goto loc_failed;

    nonce++;
  }

  return;

loc_failed:
  aergolite_free_transaction(log);
  plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;

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

SQLITE_PRIVATE int broadcast_transaction(plugin *plugin, struct transaction *txn){
  struct node *node;
  binn *map;

  SYNCTRACE("broadcast_transaction"
            " node=%d nonce=%" INT64_FORMAT " sql_count=%d\n",
            txn->node_id, txn->nonce, txn_sql_count(txn->log) );

  plugin->sync_up_state = DB_STATE_SYNCHRONIZING;

  map = binn_map();
  if( !map ){
    plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
    return SQLITE_NOMEM;
  }

  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_TRANSACTION);
  binn_map_set_int32(map, PLUGIN_NODE_ID, txn->node_id);
  binn_map_set_int64(map, PLUGIN_NONCE, txn->nonce);
  binn_map_set_list (map, PLUGIN_SQL_CMDS, txn->log);

  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      send_peer_message(node, map, NULL);
    }
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

SQLITE_PRIVATE int build_last_nonce_array_cb(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  void *authorization,
  int64 last_nonce
){
  struct plugin *plugin = (struct plugin *) arg;
  struct transaction *txn;

  SYNCTRACE("build_last_nonce_array node_id=%d last_nonce=%" INT64_FORMAT "\n",
            node_id, last_nonce);

  /* check if there are transactions from this node on the local mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->node_id==node_id && txn->block_height==0 ){
      /* add the node's last_nonce to the array */
      update_last_nonce_array(plugin, node_id, last_nonce);
      break;
    }
  }

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE int build_last_nonce_array(plugin *plugin){
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("build_last_nonce_array\n");

  if( plugin->nonces ){
    array_free(&plugin->nonces);
  }

  return aergolite_iterate_authorizations(this_node, build_last_nonce_array_cb, plugin);
}

/****************************************************************************/

SQLITE_PRIVATE int check_mempool_transaction_cb(
  void *arg,
  int node_id,
  char *pubkey,
  int pklen,
  void *authorization,
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

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE int check_mempool_transactions(plugin *plugin){
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("check_mempool_transactions\n");

  return aergolite_iterate_authorizations(this_node, check_mempool_transaction_cb, plugin);
}

/****************************************************************************/

SQLITE_PRIVATE int store_transaction_on_mempool(
  plugin *plugin, int node_id, int64 nonce, void *log, struct transaction **ptxn
){
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  char datetime[24];
  int64 tid, tnonce, last_nonce;
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

  /* check if transaction was already included on a block */
  rc = aergolite_get_authorization(this_node, node_id, NULL, NULL, NULL, &last_nonce);
  if( rc!=SQLITE_OK ) return rc;
  if( nonce<=last_nonce ){
    SYNCTRACE("store_transaction_on_mempool - transaction nonce is old\n");
    return SQLITE_INVALID;
  }

  /* check if this transaction is valid */
  rc = aergolite_verify_transaction(this_node, node_id, log, &tnonce, datetime);
  if( rc!=SQLITE_OK ) return rc;

  if( tnonce!=nonce ) return SQLITE_INVALID;

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
  strcpy(txn->datetime, datetime);

  if( !txn->log ){
    sqlite3_free(txn);
    return SQLITE_NOMEM;
  }

  if( ptxn ) *ptxn = txn;
  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE void discard_mempool_transaction(plugin *plugin, struct transaction *txn){

  SYNCTRACE("discard_mempool_transaction node_id=%d nonce=%" INT64_FORMAT "\n",
            txn->node_id, txn->nonce);

  /* remove the transaction from the mempool */
  if( txn ){
    llist_remove(&plugin->mempool, txn);
    if( txn->log ) sqlite3_free(txn->log);
    sqlite3_free(txn);
  }

}

/****************************************************************************/

SQLITE_PRIVATE int process_new_transaction(
  plugin *plugin,
  int node_id,
  int64 nonce,
  void *log,
  struct transaction **ptxn
){
  struct transaction *txn;
  int rc;

  SYNCTRACE("process_new_transaction - node=%d nonce=%" INT64_FORMAT
            " sql_count=%d\n", node_id, nonce, txn_sql_count(log) );

  /* store the transaction in the local mempool */
  rc = store_transaction_on_mempool(plugin, node_id, nonce, log, &txn);
  if( rc==SQLITE_EXISTS ){ rc = SQLITE_OK; goto loc_exit; }
  if( rc ) return rc;

  /* is it a requested transaction? */
  if( process_arrived_transaction(plugin,txn)==false ){
    /* start the timer to generate a new block */
    start_new_block_timer(plugin);
  }

loc_exit:
  if( ptxn ) *ptxn = txn;
  return rc;
}

/****************************************************************************/

/*
** A new transaction was received from a peer
*/
SQLITE_PRIVATE void on_new_remote_transaction(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  struct transaction *txn;
  int node_id;
  int64 nonce;
  void *log;

  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);
  nonce   = binn_map_int64(msg, PLUGIN_NONCE);
  log     = binn_map_list (msg, PLUGIN_SQL_CMDS);

  SYNCTRACE("on_new_remote_transaction - node_id=%d nonce=%" INT64_FORMAT
            " sql_count=%d\n", node_id, nonce, txn_sql_count(log) );

  process_new_transaction(plugin, node_id, nonce, log, NULL);

}

/****************************************************************************/

SQLITE_PRIVATE void on_local_transaction(plugin *plugin) {

  SYNCTRACE("on_local_transaction\n");

  if( plugin->sync_up_state!=DB_STATE_SYNCHRONIZING ){
    /* update the upstream state */
    plugin->sync_up_state = DB_STATE_LOCAL_CHANGES;
    /* check if already downloaded all txns */
    if( plugin->sync_down_state==DB_STATE_IN_SYNC ){
      /* send the new local transaction(s) */
      send_local_transactions(plugin);
    }
  }

}

/****************************************************************************/

// check cmd type
//
// add_node:
//   if node is connected, mark it as authorized + send all auths to him
//   send this new auth to other connected nodes
//
// remove_node: (later)

SQLITE_PRIVATE void process_new_special_transaction(plugin *plugin) {
  struct txn_list *txn;

  SYNCTRACE("process_new_special_transaction\n");

loc_next:

  sqlite3_mutex_enter(plugin->mutex);
  txn = plugin->special_txn;
  if( txn ){
    plugin->special_txn = txn->next;
  }
  sqlite3_mutex_leave(plugin->mutex);

  if( txn ){
    char pubkey[36];
    int rc, pklen;

    /* is it an authorization? */
    rc = read_authorized_pubkey(txn->log, pubkey, &pklen);
    if( rc==SQLITE_OK ){
      rc = on_new_authorization(plugin, txn->log, FALSE);
    }

    binn_free(txn->log);
    sqlite3_free(txn);

    if( rc ) return;
    goto loc_next;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void worker_thread_on_local_transaction(plugin *plugin) {

  SYNCTRACE("worker thread: on new local transaction\n");

  if( plugin->special_txn ){
    process_new_special_transaction(plugin);
  }

  on_local_transaction(plugin);

}

/****************************************************************************/

/*
** This function is called on the main thread. It must send the notification
** to the worker thread and return as fast as possible.
*/
SQLITE_API void on_new_local_transaction(void *arg, void *log) {
  plugin *plugin = (struct plugin *) arg;

  SYNCTRACE("on_new_local_transaction\n");

  if( log ){  /* special transaction */
    struct txn_list *txn = sqlite3_malloc_zero(sizeof(struct txn_list));
    if( txn ){
      txn->log = log;
      sqlite3_mutex_enter(plugin->mutex);
      llist_add(&plugin->special_txn, txn);
      sqlite3_mutex_leave(plugin->mutex);
    }else{
      binn_free(log);
    }
  }

  if( plugin->thread_active ){
    int rc, cmd = WORKER_THREAD_NEW_TRANSACTION;
    /* send command to the worker thread */
    SYNCTRACE("sending worker thread command: new local transaction\n");
    if( (rc=send_notification_to_worker(plugin, (char*)&cmd, sizeof(cmd))) < 0 ){
      SYNCTRACE("send_notification_to_worker failed: (%d) %s\n", rc, uv_strerror(rc));
    }
  }

}
