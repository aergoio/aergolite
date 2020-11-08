/*

Requests:

-> transactions
-> blocks

Each transaction/block request with its own structure,
its own timer and its own list of contacted nodes.

Send to a new random node at each 500 ms interval
until some of them sends the requested object

*/

SQLITE_PRIVATE void request_transaction_next(plugin *plugin, struct request *request);
SQLITE_PRIVATE int  request_transaction_to_node(plugin *plugin, node *node, int64 tid);
SQLITE_PRIVATE void request_transaction_timer_cb(uv_timer_t* handle);

SQLITE_PRIVATE void request_block_next(plugin *plugin, struct request *request);
SQLITE_PRIVATE int  request_block_to_node(plugin *plugin, node *node, int64 height);
SQLITE_PRIVATE void request_block_timer_cb(uv_timer_t* handle);

/****************************************************************************/

SQLITE_PRIVATE int compare_int(void *pitem1, void *pitem2){
  int item1 = *(int*)pitem1;
  int item2 = *(int*)pitem2;
  if( item2==item1 ){
    return 0;
  }else if( item2 > item1 ){
    return 1;
  }else{
    return -1;
  }
}

/****************************************************************************/

#if 0
SQLITE_PRIVATE struct node * select_random_connected_node(plugin *plugin){
  struct node *node;
  void *list;
  int rc, count, num, selected_id;

  /* build a list of connected authorized nodes ids */
  count = 0;
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ) count++;
  }
  if( count==0 ) return NULL;
  list = new_array(count, sizeof(int));
  if( !list ) return NULL;
  assert(array_count(list)==0);
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      array_append(&list, &node->id);
    }
  }
  assert(array_count(list)==count);

  /* select one node at random */
  num = random_number(0, count-1);
  assert( num>=0 && num<=count-1 );
  array_get(list, num, &selected_id);
  assert( selected_id!=0 );
  assert( in_array(list,compare_int,&selected_id) );
  assert( !in_array(excluded_nodes,compare_int,&selected_id) );
  array_free(&list);

  /* get the node object from node id */
  for( node=plugin->peers; node; node=node->next ){
    if( node->id==selected_id ){
      break;
    }
  }
  assert(node);

  return node;
}
#endif

/****************************************************************************/

SQLITE_PRIVATE struct node * select_random_connected_node_not_in_list(
  plugin *plugin,
  void *excluded_nodes
){
  struct node *node;
  void *list;
  int rc, count, num, selected_id, *pint;

  /* count the connected authorized nodes that are not
  ** in the excluded list */
  count = 0;
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized && !in_array(excluded_nodes,compare_int,&node->id) ){
      count++;
    }
  }
  if( count==0 ) return NULL;  /* no remaining connected nodes */

  /* build a list of connected authorized nodes ids that are not
  ** in the excluded list */
  list = new_array(count, sizeof(int));
  if( !list ) return NULL;  /* out of memory */
  assert(array_count(list)==0);
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized && !in_array(excluded_nodes,compare_int,&node->id) ){
      array_append(&list, &node->id);
    }
  }
  assert(array_count(list)==count);

  /* select one node at random */
  num = random_number(0, count-1);
  assert( num>=0 && num<=count-1 );
  pint = array_get(list, num);
  assert( pint );
  selected_id = *pint;
  assert( selected_id!=0 );
  assert( in_array(list,compare_int,&selected_id) );
  assert( !in_array(excluded_nodes,compare_int,&selected_id) );
  array_free(&list);

  /* get the node object from node id */
  for( node=plugin->peers; node; node=node->next ){
    if( node->id==selected_id ){
      break;
    }
  }
  assert(node);

  return node;
}

/****************************************************************************/
/** TRANSACTIONS ************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void request_transaction_finished(plugin *plugin, struct request *request){

  /* release the array */
  array_free(&request->contacted_nodes);

  /* remove it from the list now to avoid problems */
  llist_remove(&plugin->requests, request);

  /* close the timer */
  uv_close2((uv_handle_t*)&request->timer, worker_thread_on_close);

  /* the memory is released on timer close callback */

}

/****************************************************************************/

SQLITE_PRIVATE struct request * find_transaction_request(plugin *plugin, int64 tid){
  struct request *request;
  for( request=plugin->requests; request; request=request->next ){
    if( request->transaction_id==tid ) break;
  }
  return request;
}

/****************************************************************************/

SQLITE_PRIVATE bool requested_transaction_arrived(plugin *plugin, int64 tid){
  struct request *request;

  /* find the request object */
  for( request=plugin->requests; request; request=request->next ){
    if( request->transaction_id==tid ) break;
  }
  if( !request ) return false;  /* this is not a requested transaction or it already arrived before */

  /* close the request */
  request_transaction_finished(plugin, request);

  /* this is a requested transaction */
  return true;
}

/****************************************************************************/

SQLITE_PRIVATE struct request * request_transaction(plugin *plugin, int64 tid){
  struct request *request;

  SYNCTRACE("request_transaction - tid=%" INT64_FORMAT "\n", tid);
  assert( tid>0 );

  /* is there a request for this transaction already running? */
  for( request=plugin->requests; request; request=request->next ){
    if( request->transaction_id==tid ) break;
  }
  if( request ){
    SYNCTRACE("request_transaction - ALREADY EXISTS\n");
    return NULL;
  }

  /* allocate a new request object */
  request = sqlite3_malloc_zero(sizeof(struct request));
  if( !request ) return NULL;

  /* add it to the list of requests */
  llist_add(&plugin->requests, request);

  /* save the transaction id */
  request->transaction_id = tid;

  /* initialize the array of contacted nodes */
  request->contacted_nodes = new_array(plugin->total_authorized_nodes, sizeof(int));

  /* initialize the timer */
  uv_timer_init(plugin->loop, &request->timer);

  /* send the first request */
  request_transaction_next(plugin, request);

  /* activate the timer to send new requests */
  uv_timer_start(&request->timer, request_transaction_timer_cb, 500, 500);

  return request;
}

/****************************************************************************/

SQLITE_PRIVATE void request_transaction_next(plugin *plugin, struct request *request){
  struct node *node;
  int rc;

  SYNCTRACE("request_transaction_next - tid=%" INT64_FORMAT "\n",
            request->transaction_id);

loc_next:

  /* select a random connected node */
  node = select_random_connected_node_not_in_list(plugin, request->contacted_nodes);
  if( !node ){
    SYNCTRACE("request_transaction_next - no remaining nodes\n");
    request_transaction_finished(plugin, request);
    return;
  }

  /* mark the node as contacted */
  array_append(&request->contacted_nodes, &node->id);

  /* send the request to the selected node */
  rc = request_transaction_to_node(plugin, node, request->transaction_id);
  if( rc ) goto loc_next;

}

/****************************************************************************/

SQLITE_PRIVATE int request_transaction_to_node(plugin *plugin, node *node, int64 tid){
  binn *map;
  int rc=SQLITE_ERROR;

  SYNCTRACE("request_transaction - tid=%" INT64_FORMAT "\n", tid);
  assert( tid>0 );

  /* create request packet */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_GET_TRANSACTION)==FALSE ) goto loc_exit;
  if( binn_map_set_int64(map, PLUGIN_TID, tid)==FALSE ) goto loc_exit;
  //! it could be signed

  /* send the packet */
  if( send_peer_message(node, map, NULL)==FALSE ) goto loc_exit;

  rc = SQLITE_OK;

loc_exit:

  binn_free(map);
  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE void request_transaction_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  struct request *request = container_of(handle, struct request, timer);

  SYNCTRACE("request_transaction_timer_cb\n");

  request_transaction_next(plugin, request);

}

/****************************************************************************/

SQLITE_PRIVATE void on_requested_remote_transaction(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  aergolite *this_node = plugin->this_node;
  struct request *request;
  int node_id;
  int64 nonce, tid;
  void *log;
  int rc;

  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);
  nonce   = binn_map_int64(msg, PLUGIN_NONCE);
  log     = binn_map_list (msg, PLUGIN_SQL_CMDS);
  //sig   = binn_map_blob (msg, PLUGIN_SIGNATURE, NULL); it is in the log

  SYNCTRACE("on_requested_remote_transaction - node_id=%d nonce=%" INT64_FORMAT
            " log_size=%d\n", node_id, nonce, binn_size(log) );

  tid = aergolite_get_transaction_id(node_id, nonce);

  /* is this a requested transaction? */
  request = find_transaction_request(plugin, tid);
  if( !request ) return;

  /* is this transaction already included on a block? */
  if( request->height==0 ){
    /* call the same callback for new transactions */
    on_new_remote_transaction(node, msg, size);
    return;
  }

  if( request->height<0 || request->seq<=0 || !log ) return;

  /* is the transaction valid? */
  rc = aergolite_save_transaction(this_node, request->height, request->seq, node_id, nonce, log);
  if( rc ) return;

  /* discard the txn request */
  request_transaction_finished(plugin, request);

}

/****************************************************************************/
/** BLOCKS ******************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void request_block_finished(plugin *plugin, struct request *request){

  /* release the array */
  array_free(&request->contacted_nodes);

  /* remove it from the list now to avoid problems */
  llist_remove(&plugin->requests, request);

  /* close the timer */
  uv_close2((uv_handle_t*)&request->timer, worker_thread_on_close);

  /* the memory is released on timer close callback */

}

/****************************************************************************/

SQLITE_PRIVATE bool requested_block_arrived(plugin *plugin, int64 height, bool stop){
  struct request *request;

  /* find the request object */
  for( request=plugin->requests; request; request=request->next ){
    if( request->block_height==height ) break;
  }
  if( !request ) return false;  /* this is not a requested block or it already arrived before */

  if( stop ){
    /* close the request */
    request_block_finished(plugin, request);
  }

  /* this is a requested block */
  return true;
}

/****************************************************************************/

SQLITE_PRIVATE void request_block(plugin *plugin, int64 height){
  struct request *request;

  SYNCTRACE("request_block - height=%" INT64_FORMAT "\n", height);
  assert( height>0 );

  /* is there a request for this block already running? */
  for( request=plugin->requests; request; request=request->next ){
    if( request->block_height==height ) break;
  }
  if( request ){
    SYNCTRACE("request_block - ALREADY EXISTS\n");
    return;
  }

  /* allocate a new request object */
  request = sqlite3_malloc_zero(sizeof(struct request));
  if( !request ) return;

  /* add it to the list of requests */
  llist_add(&plugin->requests, request);

  /* save the block id */
  request->block_height = height;

  /* initialize the array of contacted nodes */
  request->contacted_nodes = new_array(plugin->total_authorized_nodes, sizeof(int));

  /* initialize the timer */
  uv_timer_init(plugin->loop, &request->timer);

  /* send the first request */
  request_block_next(plugin, request);

  /* activate the timer to send new requests */
  uv_timer_start(&request->timer, request_block_timer_cb, 1500, 1500);

}

/****************************************************************************/

SQLITE_PRIVATE void request_block_next(plugin *plugin, struct request *request){
  struct node *node;
  int rc;

  SYNCTRACE("request_block_next - height=%" INT64_FORMAT "\n",
            request->block_height);

loc_next:

  /* select a random connected node */
  node = select_random_connected_node_not_in_list(plugin, request->contacted_nodes);
  if( !node ){
    SYNCTRACE("request_block_next - no remaining nodes\n");
    request_block_finished(plugin, request);
    return;
  }

  /* mark the node as contacted */
  array_append(&request->contacted_nodes, &node->id);

  /* send the request to the selected node */
  rc = request_block_to_node(plugin, node, request->block_height);
  if( rc ) goto loc_next;

}

/****************************************************************************/

SQLITE_PRIVATE int request_block_to_node(plugin *plugin, node *node, int64 height){
  binn *map;
  int rc=SQLITE_ERROR;

  SYNCTRACE("request_block - height=%" INT64_FORMAT "\n", height);
  assert( height>0 );

  /* create request packet */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_GET_BLOCK)==FALSE ) goto loc_exit;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, height)==FALSE ) goto loc_exit;
  //! it could be signed

  /* send the packet */
  if( send_peer_message(node, map, NULL)==FALSE ) goto loc_exit;

  rc = SQLITE_OK;

loc_exit:

  binn_free(map);
  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE void request_block_timer_cb(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  struct request *request = container_of(handle, struct request, timer);

  SYNCTRACE("request_block_timer_cb\n");

  request_block_next(plugin, request);

}

/****************************************************************************/

/*
** This function is only used by full nodes.
** The light nodes only request state updates.
*/
SQLITE_PRIVATE void on_requested_block(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  aergolite *this_node = plugin->this_node;
  binn_iter iter;
  binn value;
  void *header, *body, *votes, *list;
  int64 height;
  int rc, seq;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);
  header = binn_map_blob(msg, PLUGIN_STATE, NULL);
  body   = binn_map_blob(msg, PLUGIN_BODY, NULL);
  votes  = binn_map_list(msg, PLUGIN_VOTES);

  SYNCTRACE("on_requested_block - height=%" INT64_FORMAT "\n", height);

  /* does it have the body? */
  if( height<=0 || !header || !body || !votes ) return;

  /* is this request still active? */
  if( requested_block_arrived(plugin,height,false)==false ) return;

  /* verify and save the block */
  rc = aergolite_save_block(this_node, header, body, votes);
  if( rc ) return;

  /* get the list of transactions ids */
  list = binn_map_list(body, BODY_TXN_IDS);

  /* request the transactions */
  seq = 0;
  binn_list_foreach(list, value){
    struct request *request;
    int64 tid = value.vint64 & 0x7fffffffffffffff; /* remove the flag of failed transaction */
    assert( value.type==BINN_INT64 );
    /* send the request */
    seq++;
    request = request_transaction(plugin, tid);
    request->height = height;
    request->seq = seq;
  }

  /* stop sending new requests for this block */
  requested_block_arrived(plugin, height, true);

  /* request the next missing block */
  request_missing_blocks(plugin, height);

}

/****************************************************************************/

SQLITE_PRIVATE void request_missing_blocks(plugin *plugin, int64 last_retrieved){
  aergolite *this_node = plugin->this_node;
  sqlite3 *db;
  sqlite3_stmt *stmt=NULL;
  char *sql;
  int64 start, end, last_block, height;
  int rc;

  SYNCTRACE("request_missing_blocks last_retrieved=%" INT64_FORMAT "\n", last_retrieved);

  if( !plugin->current_block ) return;
  last_block = plugin->current_block->height;

  if( last_retrieved==0 ){
    plugin->downloading_block_bodies = FALSE;
    last_retrieved = last_block;
  }

  db = aergolite_get_db_connection(this_node, AERGOLITE_STATE_DB);
  assert( db!=NULL );

  if( plugin->downloading_block_bodies ){
    goto loc_check_blocks_without_body;
  }

  /* find the missing ids on a sequence, from start to end */
  sql = "WITH sequence AS (SELECT $start AS x UNION SELECT x+1 FROM sequence WHERE x < $end) "
        "SELECT x FROM sequence WHERE x NOT IN (SELECT height FROM blocks)";

  /* prepare a statement for reuse with new bound values */
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if( rc!=SQLITE_OK || !stmt ) goto loc_exit;

  for(end=last_retrieved-1; end>0 && rc==SQLITE_OK; end-=512){
    start = end - 512;
    if( start<1 ) start = 1;

    sqlite3_bind_int64(stmt, 1, start);
    sqlite3_bind_int64(stmt, 2, end);

    while( (rc=sqlite3_step(stmt))==SQLITE_ROW ){
      /* found a missing block. request it to other nodes */
      height = sqlite3_column_int64(stmt, 0);
      goto loc_send_request;
    }

    if( rc==SQLITE_DONE ) rc = SQLITE_OK;
  }

  /* if execution reached this point, it either is an error or there is no
  ** missing blocks on the database */
  if( rc ) goto loc_exit;


  /* verify the chain of blocks */
  rc = aergolite_verify_history(this_node);
  if( rc ){
    sqlite3_log(SQLITE_INVALID, "invalid chain of blocks");
    goto loc_exit;
  }


  /* start checking for blocks without body */
  plugin->downloading_block_bodies = TRUE;
  last_retrieved = last_block + 1;

loc_check_blocks_without_body:

  /* find the blocks without body */
  sql = "SELECT height FROM blocks WHERE height < ? AND body IS NULL ORDER BY height DESC LIMIT 1";
  rc = aergolite_db_query_int64(&height, db, sql, "l", last_retrieved);
  if( rc==SQLITE_OK ) goto loc_send_request;

    //! or request just the body and txns??
    //! will it replace the block record with the one with the body?

  /* none found. this node is updated */
  goto loc_exit;

loc_send_request:
  /* request the block to the peers */
  request_block(plugin, height);

loc_exit:
  if( stmt ) sqlite3_finalize(stmt);

}

/****************************************************************************/

SQLITE_PRIVATE void on_get_block(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = plugin->this_node;
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  int64 height;
  struct block *block, block1={0};
  binn *map;
  int rc;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);

  SYNCTRACE("on_get_block - request from node %d - height=%" INT64_FORMAT "\n",
            node->id, height);

  if( height<=0 ) return;

  map = binn_map();
  if( !map ) return;

  if( height==current_height ){
    block = plugin->current_block;
    rc = SQLITE_OK;
  }else if( plugin->is_full_node ){
    /* load it from the database */
    block = &block1;
    rc = aergolite_get_block(this_node, height, &block->header, &block->body, &block->votes);
  }else{
    rc = SQLITE_NOTFOUND;
  }

  switch( rc ){
  case SQLITE_NOTFOUND:  /* there is no record with the given height */
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_BLOCK_NOTFOUND);
    break;
  case SQLITE_OK:
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUESTED_BLOCK);
    binn_map_set_int64(map, PLUGIN_HEIGHT, height);
    binn_map_set_blob(map, PLUGIN_HEADER, block->header, binn_size(block->header));
    binn_map_set_blob(map, PLUGIN_BODY,   block->body,   binn_size(block->body));
    //binn_map_set_map(map, PLUGIN_HEADER, block->header);
    //binn_map_set_map(map, PLUGIN_BODY, block->body);
    binn_map_set_list(map, PLUGIN_VOTES, block->votes);
    if( block==&block1 ){
      sqlite3_free(block->header);
      sqlite3_free(block->body);
      binn_free(block->votes);
    }
    break;
  default:
    sqlite3_log(rc, "on_get_block: get_block failed");
    goto loc_exit;
  }

  send_peer_message(node, map, NULL);

loc_exit:

  if( map ) binn_free(map);

}
