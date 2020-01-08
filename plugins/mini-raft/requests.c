/*

requests.c

transactions
blocks

each txn/block request with its own structure
its own timer and its own list of contacted nodes.
send to a new random node at each 500 ms interval
until some of them sends the requested object

*/

SQLITE_PRIVATE void request_transaction_next(plugin *plugin, struct request *request);
SQLITE_PRIVATE int request_transaction_to_node(plugin *plugin, node *node, int64 tid);
SQLITE_PRIVATE void request_transaction_timer_cb(uv_timer_t* handle);

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

SQLITE_PRIVATE void request_transaction(plugin *plugin, int64 tid){
  struct request *request;

  SYNCTRACE("request_transaction - tid=%" INT64_FORMAT "\n", tid);
  assert( tid>0 );

  /* is there a request for this transaction already running? */
  for( request=plugin->requests; request; request=request->next ){
    if( request->transaction_id==tid ) break;
  }
  if( request ){
    SYNCTRACE("request_transaction - ALREADY EXISTS\n");
    return;
  }

  /* allocate a new request object */
  request = sqlite3_malloc_zero(sizeof(struct request));
  if( !request ) return;

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

  SYNCTRACE("on_requested_remote_transaction\n");

  /* call the same callback for new transactions */
  on_new_remote_transaction(node, msg, size);

}

/****************************************************************************/
/** BLOCKS ******************************************************************/
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

SQLITE_PRIVATE void on_get_block(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;

  int64 height = binn_map_int64(msg, PLUGIN_HEIGHT);

  SYNCTRACE("on_get_block - request from node %d - height=%" INT64_FORMAT "\n", node->id, height);

  map = binn_map();
  if (!map) goto loc_failed;

  rc = aergolite_get_block(this_node, height, &block->header, &block->body, &block->signatures);

  switch( rc ){
  case SQLITE_NOTFOUND: /* there is no record with the given prev_tid */
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_BLOCK_NOTFOUND);
    break;
  case SQLITE_OK:
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUESTED_BLOCK);
    binn_map_set_int64(map, PLUGIN_HEIGHT, height);
    binn_map_set_blob(map, PLUGIN_HEADER, block->header, binn_size(block->header));
    binn_map_set_blob(map, PLUGIN_BODY, block->body, binn_size(block->body));
    binn_map_set_blob(map, PLUGIN_SIGNATURES, block->signatures, binn_size(block->signatures));
    break;
  default:
    sqlite3_log(rc, "on_get_block: get_block failed");
    goto loc_failed;
  }

  send_peer_message(node, map, on_data_sent);

  return;

loc_failed:

  if (map) binn_free(map);

}

#endif

/****************************************************************************/
