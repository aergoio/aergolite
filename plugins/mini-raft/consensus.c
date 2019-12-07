
SQLITE_PRIVATE int verify_last_block(plugin *plugin);

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

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
    SYNCTRACE("--- FAILED: 'requested' remote transaction arrived while this node is not synchronizing\n");
    return;
  }

  rc = on_new_remote_transaction(node, msg, size);

  if( rc==SQLITE_OK ){
    verify_last_block(plugin);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_requested_transaction_not_found(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_requested_transaction_not_found\n");

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
    SYNCTRACE("--- FAILED: 'requested' remote transaction arrived while this node is not synchronizing\n");
    return;
  }

  plugin->sync_down_state = DB_STATE_OUTDATED;

  request_state_update(plugin);

}

/****************************************************************************/
#if 0
SQLITE_PRIVATE void request_block(plugin *plugin, int64 height){
  binn *map;

  SYNCTRACE("request_block - height=%" INT64_FORMAT "\n", height);
  assert(height>0);

  if( !plugin->leader_node ) return;

  /* create request packet */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_GET_BLOCK)==FALSE ) goto loc_failed;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, height)==FALSE ) goto loc_failed;

  /* send the packet */
  if( send_peer_message(plugin->leader_node, map, on_transaction_request_sent)==FALSE ) goto loc_failed;

  binn_free(map);

  return;
loc_failed:
  if( map ) binn_free(map);
//  plugin->sync_down_state = DB_STATE_ERROR;

}

/****************************************************************************/

SQLITE_PRIVATE void on_requested_block(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_requested_block\n");

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
    SYNCTRACE("--- FAILED: 'requested' block arrived while this node is not synchronizing\n");
    return;
  }

  rc = store_new_block(node, msg, size);

  if( rc==SQLITE_OK ){
    apply_last_block(plugin);
  }

}
#endif
/****************************************************************************/

// iterate the payload to check the transactions
// download those that are not in the local mempool
// when they arrive, call fn to check if it can apply
// execute txns from the payload

SQLITE_PRIVATE int verify_block(plugin *plugin, struct block *block){
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  BOOL all_present = TRUE;
  binn_iter iter;
  binn value;
  void *list;
  int rc;

  SYNCTRACE("verify_block\n");

  /* if this node is in a state update, return */
  if( plugin->sync_down_state!=DB_STATE_IN_SYNC ) return SQLITE_ERROR;

  //block = plugin->new_block;
  if( !block ) return SQLITE_EMPTY;
  assert(block->height>0);
  plugin->new_block = block;

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

  rc = aergolite_verify_block(this_node, block->header, block->body);
  if( rc ) goto loc_failed;

  if( block->sender ){
    /* inform the sender that this node approved the block */
    //! it must be signed (?) - later
    binn *map = binn_map();
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_BLOCK_ACK);
    binn_map_set_int64(map, PLUGIN_HEIGHT, block->height);
    send_peer_message(block->sender, map, NULL);
    binn_free(map);
  }

  return SQLITE_OK;

loc_failed:
  SYNCTRACE("verify_block FAILED\n");
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

SQLITE_PRIVATE int commit_block(plugin *plugin, struct block *block){
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  binn_iter iter;
  binn value;
  void *list;
  int rc;

  SYNCTRACE("commit_block\n");

  rc = aergolite_commit_block(this_node, block->header, block->body, block->signatures);
  if( rc ) goto loc_failed;

  /* get the list of transactions ids */
  list = binn_map_list(block->body, BODY_TXN_IDS);  //  BLOCK_TRANSACTIONS);

  /* mark the used transactions on the mempool */
  binn_list_foreach(list, value) {
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->id==value.vint64 ){
        txn->block_height = block->height;
      }
    }
  }

  /* remove the old transactions from the mempool */
  binn_list_foreach(list, value) {
    for( txn=plugin->mempool; txn; txn=txn->next ){
      if( txn->block_height <= block->height - 2 ){
        discard_mempool_transaction(plugin, txn);
        break;
      }
    }
  }

  /* remove old transactions from mempool */
  check_mempool_transactions(plugin);

  /* replace the previous block by the new one */
  if( plugin->current_block ) discard_block(plugin->current_block);
  plugin->current_block = block;
  plugin->new_block = NULL;

  return SQLITE_OK;

loc_failed:
  SYNCTRACE("commit_block FAILED\n");
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
  int rc;
  rc = verify_block(plugin, block);
  if( rc==SQLITE_OK ){
    rc = commit_block(plugin, block);
  }
  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE int verify_last_block(plugin *plugin) {

  return verify_block(plugin, plugin->new_block);

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_block(node *node, void *msg, int size) {
  aergolite *this_node = node->this_node;
  plugin *plugin = node->plugin;
  struct block *block;
  int64 height;
  void *header, *body;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);
  header = binn_map_blob(msg, PLUGIN_HEADER, NULL);
  body   = binn_map_blob(msg, PLUGIN_BODY, NULL);

  SYNCTRACE("on_new_block - height=%" INT64_FORMAT "\n", height);

  /* if this node is not prepared to apply this block, do not acknowledge its receival */
  if( !plugin->current_block ){
    SYNCTRACE("on_new_block plugin->current_block==NULL\n");
    if( height!=1 ) return;
  }else if( height!=plugin->current_block->height+1 ){
    SYNCTRACE("on_new_block FAILED plugin->current_block->height=%" INT64_FORMAT "\n",
              plugin->current_block->height);
    return;
  }

  /* allocate a new block structure */
  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return;  // SQLITE_NOMEM;

  /* store the new block data */
  block->height = height;
  block->header = sqlite3_memdup(header, binn_size(header));
  block->body   = sqlite3_memdup(body,   binn_size(body));

  block->sender = node; //! DANGEROUS! it can point to released memory

  if( !block->header || !block->body ){
    SYNCTRACE("on_new_block FAILED header=%p body=%p\n", block->header, block->body);
    discard_block(block);
    return;
  }

  /* store the new block data */
  if( plugin->new_block ) discard_block(plugin->new_block);
  plugin->new_block = block;

  /* verify if the block is correct */
  verify_block(plugin, block);

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

  /* if this node is in a state update, ignore the block commit command */
  if( plugin->sync_down_state!=DB_STATE_IN_SYNC ) return;

  /* check if we have some block to be committed */
  block = plugin->new_block;
  if( !block ){
    /* the block is not on memory. request it */
    //request_block(plugin, block->height);  //! if it fails, start a state update
    request_state_update(plugin);
    return;
  }

  /* check if the local block is the expected one */
  if( block->height!=height ){
    SYNCTRACE("on_commit_block - unexpected block height - cached block height: %d\n", block->height);
    request_state_update(plugin);
    return;
  }

  /* commit the new block on this node */
  commit_block(plugin, block);
  //if( rc ) ...

}

/****************************************************************************/

SQLITE_PRIVATE bool is_next_nonce(plugin *plugin, int node_id, int64 nonce){
  struct node_nonce *item;
  int count, i;
  bool node_found = false;

  SYNCTRACE("is_next_nonce node_id=%d nonce=%" INT64_FORMAT "\n",
            node_id, nonce);

  count = array_count(plugin->nonces);
  for( i=0; i<count; i++ ){
    item = array_get(plugin->nonces, i);
    //if( item->node_id==node_id && nonce==item->last_nonce+1 ){
    if( item->node_id==node_id ){
      node_found = true;
      SYNCTRACE("is_next_nonce node_id=%d last_nonce=%" INT64_FORMAT "\n",
                node_id, item->last_nonce);
      if( nonce==item->last_nonce+1 ) return true;
    }
  }

  if( !node_found && nonce==1 ) return true;  //! workaround. remove it later!
  return false;
}

/****************************************************************************/


//! ---> it can only create it if the previous state is applied on the db!!!
//!      it must check this


SQLITE_PRIVATE struct block * create_new_block(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  struct block *block;
  int64 block_height;
  int rc, count;

  SYNCTRACE("create_new_block\n");

  /* are there unused transactions on mempool? */
  count = 0;
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->block_height==0 ) count++;
  }
  if( count==0 ) return (struct block *) -1;

  /* get the list of last_nonce for each node */
  build_last_nonce_array(plugin);

  /* get the next block height */
  if( plugin->current_block ){
    block_height = plugin->current_block->height + 1;
  }else{
    SYNCTRACE("create_new_block plugin->current_block==NULL\n");
    block_height = 1;
  }

  /* allocate a new block object */
  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return NULL;

  /* start the block creation */
  rc = aergolite_begin_block(this_node);
  if( rc ) goto loc_failed2;

  /* execute the transactions from the local mempool */
  count = 0;
loc_again:
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->block_height==0 && is_next_nonce(plugin,txn->node_id,txn->nonce) ){
      /* include this transaction on the block */
      /* no need to check the return result. if the execution failed or was rejected
      ** the nonce will be included in the block as a failed transaction */
      aergolite_execute_transaction(this_node, txn->node_id, txn->nonce, txn->log);
      update_last_nonce_array(plugin, txn->node_id, txn->nonce);
      txn->block_height = -1;
      count++;
      goto loc_again;
    }
  }
  /* reset the flag on used transactions */
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->block_height==-1 ) txn->block_height = 0;
  }

  /* if no valid transactions were found */
  if( count==0 ) goto loc_failed;

  /* finalize the block creation */
  rc = aergolite_create_block(this_node, &block->height, &block->header, &block->body);
  if( rc ) goto loc_failed2;

  array_free(&plugin->nonces);
  return block;

loc_failed:
  aergolite_rollback_block(this_node);
loc_failed2:
  SYNCTRACE("create_new_block FAILED\n");
  if( block ) sqlite3_free(block);
  array_free(&plugin->nonces);
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

  if( !plugin->is_leader ){
    SYNCTRACE("new_block_timer_cb not longer the leader node\n");
    return;
  }

  block = create_new_block(plugin);
  if( !block ){
    SYNCTRACE("create_new_block FAILED. restarting the timer\n");
    /* restart the timer */
    uv_timer_start(&plugin->new_block_timer, new_block_timer_cb, NEW_BLOCK_WAIT_INTERVAL, 0);
    return;
  }
  if( block==(struct block *)-1 ) return;

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

SQLITE_PRIVATE void start_new_block_timer(plugin *plugin) {
  if( !uv_is_active((uv_handle_t*)&plugin->new_block_timer) ){
    SYNCTRACE("start_new_block_timer\n");
    uv_timer_start(&plugin->new_block_timer, new_block_timer_cb, plugin->block_interval, 0);
  }
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

  /* mark that the commit command for this block was already sent */
  block->commit_sent = TRUE;

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

  /* if the commit command for this block was already sent */
  if( block->commit_sent ) return;

  /* increment the number of nodes that acknowledged the block */
  block->ack_count++;

  SYNCTRACE("on_node_acknowledged_block - ack_count=%d total_authorized_nodes=%d\n",
            block->ack_count, plugin->total_authorized_nodes);

  /* check if we reached the majority of the nodes */
  if( block->ack_count >= majority(plugin->total_authorized_nodes) ){
    on_acknowledged_block(plugin, block);
  }

}
