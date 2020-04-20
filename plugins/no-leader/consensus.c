
SQLITE_PRIVATE void choose_block_to_vote(plugin *plugin);
SQLITE_PRIVATE void check_for_winner_block(plugin *plugin);
SQLITE_PRIVATE int  commit_block(plugin *plugin, struct block *block);

/****************************************************************************/

SQLITE_PRIVATE BOOL has_nodes_for_consensus(plugin *plugin){
  node *node;
  int count;

  count_authorized_nodes(plugin);
  if( plugin->total_authorized_nodes<=1 ) return FALSE;

  count = 0;
  if( plugin->is_authorized ){  /* this node */
    count++;
  }
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized && node->id!=0 ) count++;
  }

  SYNCTRACE("has_nodes_for_consensus connected=%d\n", count);

  if( count<majority(plugin->total_authorized_nodes) ){
    return FALSE;
  }

  return TRUE;
}

/****************************************************************************/

SQLITE_PRIVATE bool process_arrived_transaction(plugin *plugin, struct transaction *txn){
  bool is_requested_txn;

  SYNCTRACE("process_arrived_transaction\n");

  is_requested_txn = requested_transaction_arrived(plugin, txn->id);

  /* is there any block waiting to be verified? */
  if( is_requested_txn && plugin->new_blocks ){
    /* has the block wait timer already expired? */
    if( !uv_is_active((uv_handle_t*)&plugin->block_wait_timer) ){
      /* is there a winner block? does it have all the txns? */
      choose_block_to_vote(plugin);
    }
  }

  return is_requested_txn;
}

/****************************************************************************/

#if 0
SQLITE_PRIVATE void on_requested_transaction_not_found(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_requested_transaction_not_found\n");

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
    SYNCTRACE("--- FAILED: 'requested' remote transaction arrived while this node is not synchronizing\n");
    return;
  }

  ...

}
#endif

SQLITE_PRIVATE void on_requested_transaction_not_found(plugin *plugin, int64 tid){

  SYNCTRACE("on_requested_transaction_not_found\n");

  // maybe it is disconnected from other peers...

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
    request_state_update(plugin);
  }

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void discard_new_blocks(plugin *plugin){

  SYNCTRACE("discard_new_blocks\n");

  while( plugin->new_blocks ){
    struct block *next = plugin->new_blocks->next;
    discard_block(plugin->new_blocks);
    plugin->new_blocks = next;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void discard_uncommitted_blocks(plugin *plugin){
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  struct block *block;

  SYNCTRACE("discard_uncommitted_blocks\n");

loc_again:

  for( block=plugin->new_blocks; block; block=block->next ){
    if( block->height<=current_height ){
      llist_remove(&plugin->new_blocks, block);
      discard_block(block);
      goto loc_again;
    }
  }

}

/****************************************************************************/
/****************************************************************************/

/*
** votes = [ [node_id,sig] , [node_id,sig] , [node_id,sig] ]
*/
SQLITE_PRIVATE bool add_block_vote(struct block *block, int node_id, void *sig){
  binn_iter iter;
  binn vote, *item;

  SYNCTRACE("add_block_vote\n");

  if( !block->votes ){
    block->votes = binn_list();
    if( !block->votes ) return false;
  }

  /* check if the vote is already stored */
  binn_list_foreach(block->votes, vote){
    int vote_node_id;
    assert( vote.type==BINN_LIST );
    vote_node_id = binn_list_int32(&vote, 1);
    if( vote_node_id==node_id ) return false;
  }

  /* add it to the list */
  item = binn_list();
  binn_list_add_int32(item, node_id);
  binn_list_add_blob(item, sig, 64);  //siglen);
  binn_list_add_list(block->votes, item);
  binn_free(item);

  return true;
}

/****************************************************************************/

/*
** Store the votes temporarily while the block does not arrive
*/
SQLITE_PRIVATE void store_block_vote(
  plugin *plugin, int node_id, int64 height, uchar *block_id, void *sig, int siglen
){
  struct block_vote *vote;

  SYNCTRACE("store_block_vote\n");

  /* check if the vote is already stored */
  for( vote=plugin->block_votes; vote; vote=vote->next ){
    if( vote->height==height && memcmp(vote->block_id,block_id,32)==0 &&
        vote->node_id==node_id ){
      return;
    }
  }

  /* store the new vote */

  vote = sqlite3_malloc_zero(sizeof(struct block_vote));
  if( !vote ) return;

  vote->height = height;
  vote->node_id = node_id;
  memcpy(vote->block_id, block_id, 32);
  memcpy(vote->sig, sig, siglen);

  llist_add(&plugin->block_votes, vote);

}

/****************************************************************************/

SQLITE_PRIVATE void transfer_block_votes(plugin *plugin, struct block *block){
  struct block_vote *vote;

  SYNCTRACE("transfer_block_votes\n");

  for( vote=plugin->block_votes; vote; vote=vote->next ){
    if( vote->height==block->height && memcmp(vote->block_id,block->id,32)==0 ){
      if( add_block_vote(block,vote->node_id,vote->sig) ){
        block->num_votes++;
      }
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE void discard_old_block_votes(plugin *plugin){
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  struct block_vote *vote;

  SYNCTRACE("discard_old_block_votes\n");

loc_again:

  for( vote=plugin->block_votes; vote; vote=vote->next ){
    if( vote->height < current_height ){
      llist_remove(&plugin->block_votes, vote);
      sqlite3_free(vote);
      goto loc_again;
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE void clear_block_votes(plugin *plugin){

  SYNCTRACE("clear_block_votes\n");

  while( plugin->block_votes ){
    struct block_vote *next = plugin->block_votes->next;
    sqlite3_free(plugin->block_votes);
    plugin->block_votes = next;
  }

}

/****************************************************************************/
/****************************************************************************/

#if 0  //!
SQLITE_PRIVATE void on_requested_block(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_requested_block\n");

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING && plugin->sync_down_state!=DB_STATE_IN_SYNC ){
    SYNCTRACE("--- FAILED: 'requested' block arrived while this node is not synchronizing\n");
    return;
  }

  // it can be a committed or uncommitted block(s)

  rc = store_new_block(node, msg, size);

  if( rc==SQLITE_OK ){
    apply_last_block(plugin);
  }

}
#endif

/****************************************************************************/

SQLITE_PRIVATE void rollback_block(plugin *plugin){
  aergolite *this_node = plugin->this_node;

  aergolite_rollback_block(this_node);

  plugin->open_block = NULL;

}

/****************************************************************************/

/*
** Download the transactions that are not in the local mempool
*/
SQLITE_PRIVATE int check_block_transactions(plugin *plugin, struct block *block){
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  BOOL all_present = TRUE;
  binn_iter iter;
  binn value, *map;
  void *list;
  node *node;
  int rc;

  SYNCTRACE("check_block_transactions\n");

  /* if this node is in a state update, return */
  //! if( plugin->sync_down_state!=DB_STATE_IN_SYNC ) return SQLITE_BUSY;

  if( !block ) return SQLITE_EMPTY;
  assert(block->height>0);

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

  return SQLITE_OK;

}

/****************************************************************************/

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
  //! if( plugin->sync_down_state!=DB_STATE_IN_SYNC ) return SQLITE_BUSY;

  if( !block ) return SQLITE_MISUSE;
  assert(block->height>0);
  {
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  assert(block->height==current_height+1);
  }

  /* get the list of transactions ids */
  list = binn_map_list(block->body, BODY_TXN_IDS);  //  BLOCK_TRANSACTIONS);

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
      sqlite3_log(rc, "verify_block - transaction with different result");
      aergolite_rollback_block(this_node);
      goto loc_failed;
    }
  }

  rc = aergolite_verify_block(this_node, block->header, block->body, block->id);
  if( rc ) goto loc_failed;

  plugin->open_block = block;

  SYNCTRACE("verify_block OK\n");

// case: a block arrive after the votes
  /* check if it already has sufficient votes */
//  if( block->num_votes >= majority(plugin->total_authorized_nodes) ){
    /* commit the new block on this node */
//    commit_block(plugin, block);
//  }

  return SQLITE_OK;

loc_failed:
  SYNCTRACE("verify_block FAILED\n");
  if( rc!=SQLITE_BUSY ){
    if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
      plugin->sync_down_state = DB_STATE_OUTDATED; /* it may download this block later */
    }
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

  rc = aergolite_commit_block(this_node, block->header, block->body, block->votes);
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
      if( txn->block_height>0 && txn->block_height <= block->height - 2 ){
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
  llist_remove(&plugin->new_blocks, block);
  plugin->open_block = NULL;

  /* discard other blocks */
  discard_uncommitted_blocks(plugin);

  /* discard old block votes */
  discard_old_block_votes(plugin);

  SYNCTRACE("commit_block OK\n");

  start_new_block_timer(plugin);

  return SQLITE_OK;

loc_failed:
  SYNCTRACE("commit_block FAILED\n");
  if( rc!=SQLITE_BUSY ){
    if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
      plugin->sync_down_state = DB_STATE_OUTDATED; /* it may download this block later */
    }
  }
  return rc;
}

/****************************************************************************/

// it must check if all txns are in the mempool
SQLITE_PRIVATE int apply_block(plugin *plugin, struct block *block){
  int rc;
  rc = verify_block(plugin, block);
  if( rc==SQLITE_OK ){
    rc = commit_block(plugin, block);
  }
  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE void vote_on_block(plugin *plugin, struct block *block) {
  aergolite *this_node = plugin->this_node;
  char sig[64];
  struct node *node;
  binn *map;
  int rc, siglen;

  SYNCTRACE("vote_on_block - height=%" INT64_FORMAT " wait_time=%d "
            "id=%02X%02X%02X%02X\n", block->height, block->wait_time,
            block->id[0], block->id[1], block->id[2], block->id[3]);

  /* sign the vote on this block */
  rc = aergolite_sign_raw(this_node, block->id, sig, &siglen);
  if( rc ) return;

  /* vote from this node */
  if( add_block_vote(block,plugin->node_id,sig) ){
    block->num_votes++;
  }else{
    SYNCTRACE("vote_on_block - ALREADY VOTED\n");
    return;
  }

  /* flag that this node already voted on this round */
  plugin->last_vote_height = block->height;

  /* broadcast the block vote message */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_BLOCK_VOTE)==FALSE ) goto loc_exit;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, block->height)==FALSE ) goto loc_exit;
  if( binn_map_set_int32(map, PLUGIN_NODE_ID, plugin->node_id)==FALSE ) goto loc_exit;
  if( binn_map_set_blob(map, PLUGIN_HASH, block->id, 32)==FALSE ) goto loc_exit;
  if( binn_map_set_blob(map, PLUGIN_SIGNATURE, sig, siglen)==FALSE ) goto loc_exit;
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      send_peer_message(node, map, NULL);
    }
  }
loc_exit:
  binn_free(map);

}

/****************************************************************************/

SQLITE_PRIVATE void choose_block_to_vote(plugin *plugin){
  struct block *block, *winner, *excluded=NULL;
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  int rc;

  SYNCTRACE("choose_block_to_vote\n");

  /* has this node already voted on this round? */
  if( plugin->last_vote_height==current_height+1 ) return;

  /* is it applying a state update? (some pages sent) */
  if( plugin->is_updating_state ) return;

  /* calculate the blocks wait times */
  for( block=plugin->new_blocks; block; block=block->next ){
    if( block->height==current_height+1 ){
      block->wait_time = calculate_wait_interval(block->vrf_output,
          plugin->total_authorized_nodes, plugin->block_interval);
    }
  }


loc_again:

  /* select the block with the lower wait time */
  winner = NULL;
  for( block=plugin->new_blocks; block; block=block->next ){
    if( block->height==current_height+1 ){
      assert( block->wait_time > 0 );
      if( !winner || block->wait_time < winner->wait_time ){
        winner = block;
      }
    }
  }

  if( winner ){
    if( winner!=plugin->open_block ){
      if( plugin->open_block ){
        rollback_block(plugin);
      }
      /* all transactions for this block must be present on mempool */
      /* otherwise an attacker could generate a block with a fake transaction */
      rc = check_block_transactions(plugin, winner);
      if( rc==SQLITE_OK ){
        /* verify if the block is correct */
        rc = verify_block(plugin, winner);
      }
      if( rc ){
        llist_remove(&plugin->new_blocks, winner);
        llist_add(&excluded, winner);
        goto loc_again;
      }
    }
    /* the block is OK */
    vote_on_block(plugin, winner);
  }

loc_exit:

  /* restore the list of blocks */
  //for( block=excluded; block; block=block->next ){
  //  llist_add(&plugin->new_blocks, block);
  if( excluded ){
    /* concatenate one list into another */
    llist_add(&plugin->new_blocks, excluded);
  }

  /* is there already a winner block? */
  check_for_winner_block(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void on_block_wait_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;

  SYNCTRACE("on_block_wait_timeout\n");

  uv_timer_stop(&plugin->new_block_timer);

  choose_block_to_vote(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void start_block_wait_timer(plugin *plugin){
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  /* has this node already voted on this round? */
  if( plugin->last_vote_height==current_height+1 ) return;
  /* start the block wait timer if not yet started */
  if( !uv_is_active((uv_handle_t*)&plugin->block_wait_timer) ){
    int block_wait_interval = get_block_wait_interval(plugin);
    SYNCTRACE("start_block_wait_timer\n");
    uv_timer_start(&plugin->block_wait_timer, on_block_wait_timeout, block_wait_interval, 0);
  }
}

/****************************************************************************/

SQLITE_PRIVATE void on_new_block(node *node, void *msg, int size) {
  aergolite *this_node = node->this_node;
  plugin *plugin = node->plugin;
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  unsigned char *proof, vrf_output[32];
  struct block *block;
  int64 height;
  void *header, *body;
  uchar id[32]={0};
  int rc, prooflen, wait_time;

  header = binn_map_blob(msg, PLUGIN_HEADER, NULL);
  body   = binn_map_blob(msg, PLUGIN_BODY, NULL);
  proof  = binn_map_blob(msg, PLUGIN_PROOF, &prooflen);

  /* verify the block header */
  rc = aergolite_verify_block_header(this_node, header, body, &height, id);

  SYNCTRACE("on_new_block -%s height=%" INT64_FORMAT " id=%02X%02X%02X%02X "
            "wait_time=%d\n",
            rc ? " INVALID BLOCK -" : "",
            height, id[0], id[1], id[2], id[3], wait_time);

  if( rc ) return;

  /* verify the VRF proof */
  if( proof && prooflen==81 ){
    rc = verify_proof(plugin, height, node->id, proof, prooflen, vrf_output);
  }else{
    rc = SQLITE_ERROR;
  }
  if( rc ){
    if( rc==SQLITE_INVALID ){
      SYNCTRACE("on_new_block INVALID VRF PROOF\n");
    }else{
      SYNCTRACE("on_new_block INVALID NODE\n");
    }
    return;
  }

//!  wait_time = xx

  /* check if the block is already on the list */
  for( block=plugin->new_blocks; block; block=block->next ){
    if( height==block->height && memcmp(id,block->id,32)==0 ){
      SYNCTRACE("on_new_block ALREADY ON LIST\n");
      return;
    }
  }

  /* check the block height */
  if( height<=current_height ){
    SYNCTRACE("on_new_block OLD BLOCK current_height=%" INT64_FORMAT "\n",
              current_height);
    return;
  }

#if 0
  /* if another block is open */
  if( plugin->open_block ){
    rollback_block(plugin);
  }
#endif


  /* allocate a new block structure */
  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return;  // SQLITE_NOMEM;

  /* store the new block data */
  memcpy(block->id, id, 32);
  memcpy(block->vrf_output, vrf_output, sizeof block->vrf_output);
  block->height = height;
  block->header = sqlite3_memdup(header, binn_size(header));
  block->body   = sqlite3_memdup(body,   binn_size(body));

  if( !block->header || !block->body ){
    SYNCTRACE("on_new_block FAILED header=%p body=%p\n", block->header, block->body);
    discard_block(block);
    return;
  }

  /* are there stored votes for this block? */
  transfer_block_votes(plugin, block);

  /* store the new block */
  llist_add(&plugin->new_blocks, block);


  /* download the transactions that are not in the local mempool */
  rc = check_block_transactions(plugin, block);


  /* is this node outdated? */
  if( height>current_height+1 ){
    SYNCTRACE("on_new_block OUTDATED STATE current_height=%" INT64_FORMAT "\n",
              current_height);
    request_state_update(plugin);  //! what if the block is from an attacker?
    return;
  }


  /* start the block wait timer if not yet started */
  start_block_wait_timer(plugin);

  /* transaction not present on mempool */
  if( rc ) return;

  /* sometimes the block arrives after the votes */
  check_for_winner_block(plugin);

}

/****************************************************************************/

// is there a winner block?
// if yes, does it have all the txns locally?

SQLITE_PRIVATE void check_for_winner_block(plugin *plugin) {
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  struct block *block;
  int rc;

  SYNCTRACE("check_for_winner_block\n");

  /* is it applying a state update? (some pages sent) */
  if( plugin->is_updating_state ) return;

  for( block=plugin->new_blocks; block; block=block->next ){
    /* check if the block reached the majority of the votes */
    if( block->height==current_height+1 &&
        block->num_votes >= majority(plugin->total_authorized_nodes) ){

      /* commit the new block on this node */
      /* is the winning block open? */
      if( block==plugin->open_block ){
        commit_block(plugin, block);
      }else{
        if( plugin->open_block ){
          rollback_block(plugin);
        }
        /* are all transactions for this block present on mempool? */
        rc = check_block_transactions(plugin, block);
        if( rc ) return;  /* transaction not present on mempool */
        /* verify and commit the winner block */
        apply_block(plugin, block);
      }

      break;
    }
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_block_vote(node *source_node, void *msg, int size) {
  plugin *plugin = source_node->plugin;
  aergolite *this_node = plugin->this_node;
  struct block *block;
  int64 height;
  unsigned char *id;
  char *sig;
  int rc, node_id, idlen, siglen;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);
  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);
  id = binn_map_blob(msg, PLUGIN_HASH, &idlen);
  sig = binn_map_blob(msg, PLUGIN_SIGNATURE, &siglen);

  SYNCTRACE("on_block_vote - height=%" INT64_FORMAT " id=%02X%02X%02X%02X\n",
            height, *id, *(id+1), *(id+2), *(id+3));

  if( idlen!=32 || siglen!=64 ) return;

  /* verify the signature on the vote */
  rc = aergolite_verify_raw(this_node, id, NULL, node_id, sig, siglen);
  if( rc ) return;

  /* check if we have this block locally */
  for( block=plugin->new_blocks; block; block=block->next ){
    if( block->height==height && memcmp(block->id,id,32)==0 ){
      break;
    }
  }
  if( !block ){
    int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
    SYNCTRACE("on_block_vote - BLOCK NOT FOUND\n");
    if( height>current_height ){
      /* store the vote for this block */
      store_block_vote(plugin, node_id, height, id, sig, siglen);
      //if( plugin->sync_down_state==DB_STATE_IN_SYNC ){
      //  /* the block is not on memory. request it */
      //! request_block(source_node, height, id);  //! if it fails, start a state update
      //}
    }
    if( height>current_height+1 ){
      if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
        request_state_update(plugin);
      }
    }
    return;
  }

  /* check if this vote was already counted */
  if( add_block_vote(block,node_id,sig)==false ){
    SYNCTRACE("on_block_vote - block vote already stored\n");
    return;
  }

  /* increment the number of nodes that voted on this block */
  block->num_votes++;

  SYNCTRACE("on_block_vote - num_votes=%d total_authorized_nodes=%d\n",
            block->num_votes, plugin->total_authorized_nodes);

  /* do we have a winner block? */
  check_for_winner_block(plugin);

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE int count_mempool_unused_txns(plugin *plugin){
  struct transaction *txn;
  int count = 0;
  for( txn=plugin->mempool; txn; txn=txn->next ){
    if( txn->block_height==0 ) count++;
  }
  return count;
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

SQLITE_PRIVATE struct block * create_new_block(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct transaction *txn;
  struct block *block;
  int64 block_height;
  int rc, count;

  SYNCTRACE("create_new_block\n");

  /* are there unused transactions on mempool? */
  if( count_mempool_unused_txns(plugin)==0 ||
      !has_nodes_for_consensus(plugin)
  ){
    return (struct block *) -1;
  }

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
      rc = aergolite_execute_transaction(this_node, txn->node_id, txn->nonce, txn->log);
      if( rc==SQLITE_PERM ) continue;
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
  rc = aergolite_create_block(this_node, &block->height, &block->header, &block->body, block->id);
  if( rc ) goto loc_failed2;

  /* save the random wait interval and proof */
  memcpy(block->vrf_proof, plugin->block_vrf_proof, sizeof block->vrf_proof);
  memcpy(block->vrf_output, plugin->block_vrf_output, sizeof block->vrf_output);

  array_free(&plugin->nonces);
  plugin->last_created_block_height = block->height;
  SYNCTRACE("create_new_block OK\n");
  return block;

loc_failed:
  aergolite_rollback_block(this_node);
loc_failed2:
  SYNCTRACE("create_new_block FAILED\n");
  if( block ) discard_block(block);
  array_free(&plugin->nonces);
  return NULL;
}

/****************************************************************************/

/*
** -start a db transaction
** -execute the transactions from the local mempool (without the BEGIN and COMMIT)
** -track which db pages were modified and their hashes
** -create a "block" with the transactions ids (and page hashes)
** -roll back the database transaction
** -reset the block num_votes
** -broadcast the block to the peers
*/
SQLITE_PRIVATE void new_block_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;
  struct block *block;

  SYNCTRACE("new_block_timer_cb\n");

  /* if another block is open */
  if( plugin->open_block ){
    rollback_block(plugin);
  }

  block = create_new_block(plugin);
  if( !block ){
    SYNCTRACE("create_new_block FAILED. restarting the timer\n");
    /* restart the timer */
    uv_timer_start(&plugin->new_block_timer, new_block_timer_cb, plugin->block_interval, 0);
    return;
  }
  if( block==(struct block *)-1 ) return;

  /* store the new block */
  llist_add(&plugin->new_blocks, block);
  /* save the currently open block */
  plugin->open_block = block;

  /* broadcast the block to the peers */
  broadcast_new_block(plugin, block);

  /* start the block wait timer if not yet started */
  start_block_wait_timer(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void start_new_block_timer(plugin *plugin) {
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  /* if this node already generated a block for this round
  ** and it is not committed yet */
  if( plugin->last_created_block_height==current_height+1 ) return;
  if( count_mempool_unused_txns(plugin)==0 ) return;
  if( !has_nodes_for_consensus(plugin) ) return;
  if( !uv_is_active((uv_handle_t*)&plugin->new_block_timer) ){
    int interval = calculate_node_wait_interval(plugin, current_height+1);
    SYNCTRACE("start_new_block_timer interval=%d\n", interval);
    uv_timer_start(&plugin->new_block_timer, new_block_timer_cb, interval, 0);
  }
}

/****************************************************************************/

SQLITE_PRIVATE binn* encode_new_block(plugin *plugin, struct block *block) {
  binn *map;
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_BLOCK)==FALSE ) goto loc_failed;
  if( binn_map_set_blob(map, PLUGIN_HEADER, block->header, binn_size(block->header))==FALSE ) goto loc_failed;
  if( binn_map_set_blob(map, PLUGIN_BODY, block->body, binn_size(block->body))==FALSE ) goto loc_failed;
  if( binn_map_set_blob(map, PLUGIN_PROOF, block->vrf_proof, sizeof block->vrf_proof)==FALSE ) goto loc_failed;
  return map;
loc_failed:
  if( map ) binn_free(map);
  return NULL;
}

/****************************************************************************/

SQLITE_PRIVATE void send_new_block(plugin *plugin, node *node, struct block *block) {
  binn *map;

  if( !block ) return;

  SYNCTRACE("send_new_block - height=%" INT64_FORMAT " id=%02X%02X%02X%02X\n",
            block->height, block->id[0], block->id[1], block->id[2], block->id[3]);

  map = encode_new_block(plugin, block);
  if( map ){
    send_peer_message(node, map, NULL);
    binn_free(map);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void send_new_blocks(plugin *plugin, node *node) {
  struct block *block;

  if( !node ) return;

  SYNCTRACE("send_new_blocks - node=%d\n", node->id);

  for( block=plugin->new_blocks; block; block=block->next ){
    send_new_block(plugin, node, block);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void send_block_vote(
  plugin *plugin,
  node *node,
  int64 height,
  void *block_id,
  int node_id,
  void *sig
){
  binn *map;
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_BLOCK_VOTE)==FALSE ) goto loc_exit;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, height)==FALSE ) goto loc_exit;
  if( binn_map_set_int32(map, PLUGIN_NODE_ID, node_id)==FALSE ) goto loc_exit;
  if( binn_map_set_blob(map, PLUGIN_HASH, block_id, 32)==FALSE ) goto loc_exit;
  if( binn_map_set_blob(map, PLUGIN_SIGNATURE, sig, 64)==FALSE ) goto loc_exit;
  send_peer_message(node, map, NULL);
loc_exit:
  binn_free(map);
}

/****************************************************************************/

//! on gossip: these messages should not be resent to other nodes

SQLITE_PRIVATE void send_block_votes(plugin *plugin, node *node) {
  struct block *block;
  struct block_vote *vote;

  if( !node ) return;

  SYNCTRACE("send_block_votes - node=%d\n", node->id);

  /* send the votes for arrived blocks */
  for( block=plugin->new_blocks; block; block=block->next ){
    binn_iter iter;
    binn item;
    binn_list_foreach(block->votes, item){
      int node_id;
      unsigned char *sig;
      assert( item.type==BINN_LIST );
      node_id = binn_list_int32(&item, 1);
      sig = binn_list_blob(&item, 2, NULL);
      send_block_vote(plugin, node, block->height, block->id, node_id, sig);
    }
  }

  /* send the votes for not yet arrived blocks */
  for( vote=plugin->block_votes; vote; vote=vote->next ){
    send_block_vote(plugin, node, vote->height, vote->block_id, vote->node_id, vote->sig);
  }

}

/****************************************************************************/

SQLITE_PRIVATE int broadcast_new_block(plugin *plugin, struct block *block) {
  struct node *node;
  binn *map;

  SYNCTRACE("broadcast_new_block - height=%" INT64_FORMAT " id=%02X%02X%02X%02X\n",
            block->height, block->id[0], block->id[1], block->id[2], block->id[3]);

  map = encode_new_block(plugin, block);
  if( !map ) return SQLITE_NOMEM;

  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      send_peer_message(node, map, NULL);
    }
  }

  binn_free(map);
  return SQLITE_OK;
}
