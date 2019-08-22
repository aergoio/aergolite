
SQLITE_PRIVATE int apply_last_block(plugin *plugin);

/****************************************************************************/

SQLITE_PRIVATE void on_transaction_request_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_transaction_request_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE void request_transaction(plugin *plugin, int64 tid){
  binn *map;

  SYNCTRACE("request_transaction - tid=%" INT64_FORMAT "\n", tid);
  assert(tid>0);

  if( !plugin->leader_node ) return;

  /* create request packet */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_GET_TRANSACTION)==FALSE ) goto loc_failed;
  if( binn_map_set_int64(map, PLUGIN_TID, tid)==FALSE ) goto loc_failed;

  /* send the packet */
  if( send_peer_message(plugin->leader_node, map, on_transaction_request_sent)==FALSE ) goto loc_failed;

  binn_free(map);

  return;
loc_failed:
  if( map ) binn_free(map);
//  plugin->sync_down_state = DB_STATE_ERROR;

}

/****************************************************************************/

SQLITE_PRIVATE void on_requested_remote_transaction(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_requested_remote_transaction\n");

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
    SYNCTRACE("--- FAILED: 'requested' remote transaction while this node is not synchronizing\n");
    return;
  }

  rc = on_new_remote_transaction(node, msg, size);

  if( rc==SQLITE_OK ){
    apply_last_block(plugin);
  }

}

/****************************************************************************/

// iterate the payload to check the transactions
// download those that are not in the local mempool
// when they arrive, call fn to check if it can apply
// execute txns from the payload

SQLITE_PRIVATE int apply_last_block(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct block *block;
  struct transaction *txn;
  BOOL all_present = TRUE;
  binn_iter iter;
  binn value;
  void *list;
  int rc;

  SYNCTRACE("apply_last_block\n");

  block = plugin->new_block;
  if( !block ) return SQLITE_EMPTY;

  /* get the list of transactions ids */
  list = binn_map_list(block->body, BODY_TXN_IDS);  //  BLOCK_TRANSACTIONS);

  /* check whether all the transactions are present on the local mempool */
  binn_list_foreach(list, value){
    int64 txn_id = value.vint64;
    assert( value.type==BINN_INT64 );
    /* remove the flag of failed transaction */
    txn_id &= 0x7fffffffffffffff;
    /* check the transaction in the mempool */
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->id==txn_id ) break;
    }
    if( !txn ){
      /* transaction not present in the local mempool */
      all_present = FALSE;
      /* to avoid making a second request for non-arrived txns */
      if( !block->downloading_txns ){
        request_transaction(plugin, txn_id);
      }
    }
  }

  block->downloading_txns = !all_present;

  if( !all_present ) return SQLITE_BUSY;

  /* start a new block */
  rc = aergolite_begin_block(this_node);
  if( rc ) goto loc_failed;

  /* execute the transactions from the local mempool */
  binn_list_foreach(list, value) {
    int64 txn_id = value.vint64 & 0x7fffffffffffffff;
    /* x */
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->id==txn_id ) break;
    }
    /* x */
    rc = aergolite_execute_transaction(this_node, txn->node_id, txn->nonce, txn->log);
    if( rc==SQLITE_BUSY ){  /* try again later */
      aergolite_rollback_block(this_node);
      return rc;
    }
    if( (rc!=SQLITE_OK) != (value.vint64<0) ){
      sqlite3_log(rc, "apply_block - transaction with different result");
      aergolite_rollback_block(this_node);
      goto loc_failed;
    }
  }

  rc = aergolite_apply_block(this_node, block->header, block->body, block->signatures);
  //rc = aergolite_apply_new_state(this_node, state->header, state->payload);
  if( rc ) goto loc_failed;

  /* remove the used transactions from the mempool */
  binn_list_foreach(list, value) {
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->id==value.vint64 ){
        discard_mempool_transaction(plugin, txn);
        break;
      }
    }
  }

  /* replace the previous block by the new one */
  if( plugin->current_block ) discard_block(plugin->current_block);
  plugin->current_block = block;
  plugin->new_block = NULL;

  return SQLITE_OK;

loc_failed:
// close connection?
// or try again? use a timer?
  if( rc!=SQLITE_BUSY ){
    plugin->sync_down_state = DB_STATE_OUTDATED; /* it may download this block later */
    discard_block(block);
    plugin->new_block = NULL;
  }
  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE int apply_block(plugin *plugin, struct block *block){

  plugin->new_block = block;

  return apply_last_block(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_block(node *node, void *msg, int size) {
  aergolite *this_node = node->this_node;
  plugin *plugin = node->plugin;
  struct block *block;
  int64 height;
  void *header, *body;
  binn *map;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);
  header = binn_map_blob(msg, PLUGIN_HEADER, NULL);
  body   = binn_map_blob(msg, PLUGIN_BODY, NULL);

  SYNCTRACE("on_new_block - height=%" INT64_FORMAT "\n", height);

  /* if this node is not prepared to apply this block, do not acknowledge its receival */
  if( plugin->current_block && height!=plugin->current_block->height+1 ){
    if( plugin->current_block ){
      SYNCTRACE("on_new_block FAILED plugin->current_block->height=%" INT64_FORMAT "\n", plugin->current_block->height);
    }else{
      SYNCTRACE("on_new_block FAILED plugin->current_block==NULL\n");
    }
    return;
  }

  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return;  // SQLITE_NOMEM;

  block->height = height;
  block->header = sqlite3_memdup(header, binn_size(header));
  block->body   = sqlite3_memdup(body,   binn_size(body));

  if( !block->header || !block->body ){
    SYNCTRACE("on_new_block FAILED header=%p body=%p\n", block->header, block->body);
    discard_block(block);
    return;
  }

  if( plugin->new_block ) discard_block(plugin->new_block);
  plugin->new_block = block;

  map = binn_map();
  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_BLOCK_ACK);
  binn_map_set_int64(map, PLUGIN_HEIGHT, block->height);
  send_peer_message(node, map, NULL);
  binn_free(map);

}

/****************************************************************************/

SQLITE_PRIVATE void on_commit_block(node *node, void *msg, int size) {
  aergolite *this_node = node->this_node;
  plugin *plugin = node->plugin;
  struct block *block;
  int64 height;
  //uchar *hash;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);
  //hash = binn_map_blob (msg, PLUGIN_HASH, NULL);  //&hash_size);

  SYNCTRACE("on_commit_block - height=%" INT64_FORMAT "\n", height);

  block = plugin->new_block;
  if( !block ) return;

  if( block->height!=height ){
    SYNCTRACE("on_commit_block - NOT FOUND\n");
    return;
  }

  /* apply the new block on this node */
  apply_block(plugin, block);

}

/****************************************************************************/


//! ---> it can only create it if the previous state is applied on the db!!!
//!      it must check this


SQLITE_PRIVATE struct block * create_new_block(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  struct block *block;
  int rc;

  SYNCTRACE("create_new_block\n");

  if( plugin->mempool==NULL ) return NULL;

  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return NULL;

  /* start a db transaction */
  rc = aergolite_begin_block(this_node);
  if( rc ) goto loc_failed;

  /* execute the transactions from the local mempool */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    /* include this transaction on the block */
    aergolite_execute_transaction(this_node, txn->node_id, txn->nonce, txn->log);
    /* no need to check the return result. if the execution failed or was rejected
    ** the nonce will be included in the block as a failed transaction */
  }

  rc = aergolite_create_block(this_node, &block->height, &block->header, &block->body);
  if( rc ) goto loc_failed;

  return block;

loc_failed:

  if( block ) sqlite3_free(block);
  return NULL;

#if 0

  // --- or if having the mempool implemented on the core:

  binn *block = NULL;
  binn *payload = NULL;

  rc = aergolite_create_block(this_node, &block, &payload);


  // first check if all txns are in the mempool, download those that aren't, and then:
  rc = aergolite_apply_block(this_node, block, payload);  // apply_new_state

#endif

}

/****************************************************************************/

/*
** Used by the leader.
** -start a db transaction
** -execute the transactions from the local mempool (without the BEGIN and COMMIT)
** LATER: -track which db pages were modified and their hashes
** -create a "block" with the transactions ids (and page hashes)
** -roll back the database transaction
** -reset the block ack_count
** -broadcast the block to the peers
*/
SQLITE_PRIVATE void new_block_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;
  struct block *block;

  SYNCTRACE("new_block_timer_cb\n");

  block = create_new_block(plugin);
  if( !block ){
    /* restart the timer */
    uv_timer_start(&plugin->new_block_timer, new_block_timer_cb, NEW_BLOCK_WAIT_INTERVAL, 0);
    return;
  }

  /* store the new block */
  //llist_add(&plugin->blocks, block);
  plugin->new_block = block;

  /* broadcast the block to the peers */
  block->ack_count = 1;  /* ack by this node */
  broadcast_new_block(plugin, block);

// if it fails, it can use a timer to try later
// but if the connection is down probably it will have to discard the block

}

/****************************************************************************/

/*
** Used by the leader.
*/
SQLITE_PRIVATE int broadcast_new_block(plugin *plugin, struct block *block) {
  struct node *node;
  binn *map;

  SYNCTRACE("broadcast_new_block - height=%" INT64_FORMAT "\n",
            block->height);

  /* signal other peers that there is a new transaction */
  map = binn_map();
  if( !map ) return SQLITE_BUSY;  /* flag to retry the command later */

  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_BLOCK);
  binn_map_set_int64(map, PLUGIN_HEIGHT, block->height);
  binn_map_set_blob(map, PLUGIN_HEADER, block->header, binn_size(block->header));
  binn_map_set_blob(map, PLUGIN_BODY, block->body, binn_size(block->body));

  for( node=plugin->peers; node; node=node->next ){
    send_peer_message(node, map, NULL);
  }

  binn_free(map);
  return SQLITE_OK;
}

/****************************************************************************/

/*
** Used by the leader.
*/
// signed block, signed state, state commit
SQLITE_PRIVATE int broadcast_block_commit(plugin *plugin, struct block *block) {
  struct node *node;
  binn *map;

  SYNCTRACE("broadcast_block_commit - height=%" INT64_FORMAT "\n",
            block->height);

  /* signal other peers that there is a new transaction */
  map = binn_map();
  if( !map ) return SQLITE_BUSY;  /* flag to retry the command later */

  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_COMMIT_BLOCK);
  binn_map_set_int64(map, PLUGIN_HEIGHT, block->height);
  //binn_map_set_blob(map, PLUGIN_HASH, block->hash, SHA256_BLOCK_SIZE);

  for( node=plugin->peers; node; node=node->next ){
    send_peer_message(node, map, NULL);
  }

  binn_free(map);
  return SQLITE_OK;
}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void on_acknowledged_block(plugin *plugin, struct block *block) {
  int rc;

  SYNCTRACE("on_acknowledged_block - height=%" INT64_FORMAT "\n", block->height);

  /* send command to apply the new block */  //! -- is this needed?  it depends on enough signatures
  rc = broadcast_block_commit(plugin, block);
  if( rc ){
    /* discard the block */
    discard_block(block);
    plugin->new_block = NULL;
    return;
  }

  /* apply the new block on this node */
  apply_block(plugin, block);

}

/****************************************************************************/

//! ack or signed?

SQLITE_PRIVATE void on_node_acknowledged_block(node *source_node, void *msg, int size) {
  plugin *plugin = source_node->plugin;
  aergolite *this_node = source_node->this_node;
  struct block *block;
  int64 height;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);

  SYNCTRACE("on_node_acknowledged_block - height=%" INT64_FORMAT "\n", height);

  block = plugin->new_block; //! ??
  if( !block ) return;  /* already committed */

  if( block->height!=height ){
    SYNCTRACE("on_node_acknowledged_block - NOT FOUND\n");
    return;
  }

  /* increment the number of nodes that acknowledged the block */
  block->ack_count++;

  SYNCTRACE("on_node_acknowledged_block - ack_count=%d total_known_nodes=%d\n",
            block->ack_count, plugin->total_known_nodes);

  /* check if we reached the majority of the nodes */
  if( block->ack_count >= majority(plugin->total_known_nodes) ){
    on_acknowledged_block(plugin, block);
  }

}
