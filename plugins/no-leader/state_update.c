/*

protocol:

  send the request to all (or most) peers
  one at a time, selecting the node at random (check if already contacted)
  wait for the answer or timeout
  continue with the next one until majority is reached  (or until enough votes for a new block arrives)

  pro: it scales better
  pro: it may be safer, as the node can compare the result from many nodes

or:

  send a broadcast message to all peers requesting the last height number
  activate a timer to wait for answers
  when all answered OR timeout reached:
    order by the higher block height
    request the state update from the node with higher block height
      if it fails, try with the next one
    if they all have the same block height as this node, mark as updated

  problem: when connecting to many nodes, at the beginning it has less connections


if it receives enough votes for a new block, and it is at the same height (new block height - 1)
then consider that it is up-to-date
and apply the new block


when a new block arrives:
  save it, but do not verify it (unless if this node is at the previous height)
  and do not start the timers (wait_bock and vote)
when a vote arrives:
  store the vote in the plugin OR in the block
  if the amount of votes reaches majority (and it is previous height):
    verify the block too and apply it - also mark as IN_SYNC

vars:
  plugin->in_state_update
and
  plugin->is_updating_state  ->  plugin->is_applying_new_state

*/

SQLITE_PRIVATE void request_state_update_next(plugin *plugin);
SQLITE_PRIVATE int  request_state_update_to_node(plugin *plugin, node *to_node);
SQLITE_PRIVATE void state_update_finished(plugin *plugin);
SQLITE_PRIVATE void on_state_update_node_timeout(uv_timer_t* handle);

/****************************************************************************/

SQLITE_PRIVATE int load_current_state(plugin *plugin) {
  aergolite *this_node = plugin->this_node;
  struct block *block = NULL;
  int rc;

  SYNCTRACE("load_current_state\n");

  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) return SQLITE_NOMEM;

  /* load and verify the current local database state */
  rc = aergolite_load_current_state(this_node, &block->height,
            &block->header, &block->body, &block->signatures);
  if( rc ){
    sqlite3_free(block);
    plugin->current_block = NULL;
    plugin->sync_down_state = DB_STATE_ERROR;
    return rc;
  }

  /* store the current block */
  plugin->current_block = block;

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE void on_state_update_request_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_state_update_request_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

/*
** Also called when there is enough block votes for a block that is
** the next to the current applied block.
*/
SQLITE_PRIVATE void state_update_finished(plugin *plugin){

  SYNCTRACE("state_update_finished\n");

  uv_timer_stop(&plugin->state_update_timer);

  array_free(&plugin->state_update_contacted_nodes);

  plugin->contacted_node_id = 0;

  /* update the state */
  if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING ){
    plugin->sync_down_state = DB_STATE_IN_SYNC;
  }

  // this is in a timer:
  /* start sending the local transactions */
  //if( plugin->sync_up_state!=DB_STATE_SYNCHRONIZING && plugin->sync_up_state!=DB_STATE_IN_SYNC ){
  //  start_upstream_db_sync(plugin);
  //}

}

/****************************************************************************/

/* update this node's db state with the peers' current state  (sync down) */
/* if the this node's current state is invalid or empty, download a new db from the peers */

SQLITE_PRIVATE void request_state_update(plugin *plugin) {
  int64 current_height;

  SYNCTRACE("request_state_update\n");

  if( plugin->sync_down_state==DB_STATE_SYNCHRONIZING ){
    SYNCTRACE("request_state_update ALREADY SENT\n");
    return;
  }

  if( !plugin->peers ){
    plugin->sync_down_state = DB_STATE_UNKNOWN;
    return;
  }

  plugin->sync_down_state = DB_STATE_SYNCHRONIZING;

  plugin->state_update_errors = 0;

  request_state_update_next(plugin);

}

/****************************************************************************/

/*

also called when:
-the previous contacted node answered
-a timeout reached for the previous node to answer

  check if majority of nodes were already contacted

  select one node at random
  check if already contacted
    if yes, repeat
  send the request to the selected node
  activate the timer to wait for answer

  (wait for the answer or timeout)

  continue with the next one until majority is reached  (or until enough votes for a new block arrives)

*/
SQLITE_PRIVATE void request_state_update_next(plugin *plugin) {
  struct node *node;
  int rc, count;

  SYNCTRACE("request_state_update_next\n");

  assert( plugin->sync_down_state==DB_STATE_SYNCHRONIZING );

loc_next:

  /* check if majority of nodes were already contacted */
  count_authorized_nodes(plugin);  /* including off-line nodes */
  count = array_count(plugin->state_update_contacted_nodes) - plugin->state_update_errors;
  if( count >= majority(plugin->total_authorized_nodes) ){
    SYNCTRACE("request_state_update_next - majority already contacted\n");
    state_update_finished(plugin);
    return;
  }

  /* create the array of contacted nodes */
  if( !plugin->state_update_contacted_nodes ){
    plugin->state_update_contacted_nodes = new_array(plugin->total_authorized_nodes, sizeof(int));
  }

  /* select a random connected node */
  node = select_random_connected_node_not_in_list(plugin, plugin->state_update_contacted_nodes);
  if( !node ){
    SYNCTRACE("request_state_update_next - no remaining nodes\n");
    state_update_finished(plugin);
    return;
  }

  /* mark the node as contacted */
  array_append(&plugin->state_update_contacted_nodes, &node->id);

  /* send the request to the selected node */
  rc = request_state_update_to_node(plugin, node);
  if( rc ) goto loc_next;

  /* activate the timer to wait for answer */
  uv_timer_start(&plugin->state_update_timer, on_state_update_node_timeout, 3000, 0);
  // if the node answered with pages, restart the timer
  // if the node answered with finish, disable the timer

}

/****************************************************************************/

SQLITE_PRIVATE int request_state_update_to_node(plugin *plugin, node *to_node) {
  int64 current_height;
  //char *state_hash;
  binn *map;

  assert( plugin->sync_down_state==DB_STATE_SYNCHRONIZING );

  if( !to_node ){
    return SQLITE_ERROR;
  }

  if( plugin->current_block ){
    current_height = plugin->current_block->height;
    //state_hash = plugin->current_block->state_hash;
  }else{
    current_height = 0;
    //state_hash = NULL;
  }

  SYNCTRACE("request_state_update_to_node - current_height=%" INT64_FORMAT "\n", current_height);

  /* create the request packet */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUEST_STATE_DIFF)==FALSE ) goto loc_failed;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, current_height)==FALSE ) goto loc_failed;
  //if( state_hash ){
  //  if( binn_map_set_blob(map, STATE_HASH, state_hash, SHA256_BLOCK_SIZE)==FALSE ) goto loc_failed;
  //}

  /* send the packet */
  if( send_peer_message(to_node, map, on_state_update_request_sent)==FALSE ) goto loc_failed;

  binn_free(map);

  /* save which is the node the request was sent to */
  plugin->contacted_node_id = to_node->id;

  return SQLITE_OK;

loc_failed:
  if( map ) binn_free(map);
  plugin->state_update_errors++;
  return SQLITE_ERROR;
}

/****************************************************************************/

/* 
** The remote node did not answer in time
*/
SQLITE_PRIVATE void on_state_update_node_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  aergolite *this_node = plugin->this_node;

  SYNCTRACE("on_state_update_node_timeout\n");

  assert( plugin->sync_down_state==DB_STATE_SYNCHRONIZING );

  /* is it in the middle of an update? (some pages sent) */
  if( plugin->is_updating_state ){
    aergolite_cancel_state_update(this_node);
    plugin->is_updating_state = false;
  }

  plugin->state_update_errors++;

  request_state_update_next(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void on_update_db_page(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  unsigned int pgno;
  char *data;
  int64 height;
  int rc;

  height = binn_map_uint64(msg, PLUGIN_HEIGHT);
  pgno = binn_map_uint32(msg, PLUGIN_PGNO);
  data = binn_map_blob(msg, PLUGIN_DBPAGE, &size);

  SYNCTRACE("on_update_db_page - pgno: %d  size: %d\n", pgno, size);

  /* ignore messages from invalid nodes */
  if( node->id!=plugin->contacted_node_id ){
    SYNCTRACE("on_update_db_page - message coming from invalid node: %d\n", node->id);
    return;
  }

  /* check the block height */
  if( height<=current_height ){
    SYNCTRACE("on_update_db_page - OUTDATED UPDATE height=%" INT64_FORMAT
    "current_height=%" INT64_FORMAT "\n", height, current_height);
    return;
  }

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ){
    sqlite3_log(1, "on_update_db_page FAILED - the state is not synchronizing");
    //request_state_update(plugin);  -- the apply msg from the previous request can cause problems
    return;
  }

  if( plugin->open_block){
    rollback_block(plugin);
  }

  if( !plugin->is_updating_state ){
    /* start the state update */
    rc = aergolite_begin_state_update(this_node);
    if( rc ) goto loc_failed;
    plugin->is_updating_state = true;
  }

  /* apply the received page on the local database */
  rc = aergolite_update_db_page(this_node, pgno, data, size);
  if( rc!=SQLITE_OK ){
    sqlite3_log(1, "on_update_db_page - save page failed - pgno: %u", pgno);
    aergolite_cancel_state_update(this_node);
    goto loc_failed;
  }

  /* update the timer to wait for the next message */
  uv_timer_start(&plugin->state_update_timer, on_state_update_node_timeout, 3000, 0);

  return;

loc_failed:
  plugin->is_updating_state = false;
  plugin->state_update_errors++;
  /* disable the timer */
  uv_timer_stop(&plugin->state_update_timer);
  /* check on next node */
  request_state_update_next(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE void on_apply_state_update(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int64 current_height = plugin->current_block ? plugin->current_block->height : 0;
  void *header, *body, *signatures, *mod_pages;
  int64 height;
  struct block *block = NULL;
  int rc;

  height = binn_map_uint64(msg, PLUGIN_HEIGHT);
  header = binn_map_map(msg, PLUGIN_STATE);
  //body = binn_map_blob(msg, PLUGIN_BODY, &body_size);
  signatures = binn_map_list(msg, PLUGIN_SIGNATURES);
  mod_pages = binn_map_list(msg, PLUGIN_MOD_PAGES);

  SYNCTRACE("on_apply_state_update - height: %" INT64_FORMAT
            " modified pages: %d\n", height, binn_count(mod_pages));

  assert(height>0);
  assert(header);
  //assert(signatures);
  assert(mod_pages);
  assert(binn_count(mod_pages)>0);

  /* ignore messages from invalid nodes */
  if( node->id!=plugin->contacted_node_id ){
    SYNCTRACE("on_apply_state_update - message coming from invalid node: %d\n", node->id);
    return;
  }

  /* check the block height */
  if( height<=current_height ){
    SYNCTRACE("on_apply_state_update - OUTDATED UPDATE height=%" INT64_FORMAT
    "current_height=%" INT64_FORMAT "\n", height, current_height);
    return;
  }

  if( plugin->sync_down_state!=DB_STATE_SYNCHRONIZING ||
      !plugin->is_updating_state ){
    sqlite3_log(1, "on_apply_state_update FAILED - the current node is not synchronizing");
    //request_state_update(plugin);
    return;
  }

  /* commit the new state */
  //rc = aergolite_apply_state_update(this_node, header, body, signatures);
  rc = aergolite_apply_state_update(this_node, header, signatures, mod_pages);
  if( rc ) goto loc_failed;

  /* keep the new state on the memory */
  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) goto loc_failed2;

  block->height = height;
  block->header = sqlite3_memdup(header, binn_size(header));
  //block->body = sqlite3_memdup(body, body_size);  // needed on full nodes?
  block->signatures = sqlite3_memdup(signatures, binn_size(signatures));

  if( !block->header ) goto loc_failed2;
//  if( !block->signatures ) goto loc_failed2;

  /* replace the previous block by the new one */
  discard_block(plugin->current_block);
  plugin->current_block = block;

  /* discard any new incoming block */
  discard_uncommitted_blocks(plugin);
  //plugin->open_block = NULL;

  /* remove old transactions from mempool */
  check_mempool_transactions(plugin);

  SYNCTRACE("on_apply_state_update - OK\n");

loc_exit:
  plugin->is_updating_state = false;
  /* disable the timer */
  uv_timer_stop(&plugin->state_update_timer);
  /* check on next node */
  request_state_update_next(plugin);
  return;

loc_failed:
  SYNCTRACE("on_apply_state_update - FAILED\n");
  aergolite_cancel_state_update(this_node);
  plugin->state_update_errors++;
  goto loc_exit;

loc_failed2:
  SYNCTRACE("on_apply_state_update - FAILED 2\n");
  /* force to reload the current state */
  discard_block(plugin->current_block);
  plugin->current_block = NULL;
  plugin->sync_down_state = DB_STATE_UNKNOWN; // -- use this flag to reload state?  if it is out of memory, then it may not be able to relaod it... it should return error code/message, and not consider current_block_height==0
  goto loc_exit;

}

/****************************************************************************/

SQLITE_PRIVATE void on_uptodate_message(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_uptodate_message\n");

  /* ignore messages from invalid nodes */
  if( node->id!=plugin->contacted_node_id ){
    SYNCTRACE("on_uptodate_message - message coming from invalid node: %d\n", node->id);
    return;
  }

  /* disable the timer */
  uv_timer_stop(&plugin->state_update_timer);

  /* check on next node */
  request_state_update_next(plugin);

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE int compare_pgno(void *item1, void *item2){
  Pgno existing = *(Pgno*)item1;
  Pgno new = *(Pgno*)item2;
  if( new==existing ){
    return 0;
  }else if( new > existing ){
    return 1;
  }else{
    return -1;
  }
}

// what if the nodes use different base dbs?
// 1. they could check the base db, and:
//  1-a. do not update
//  1-b. the db is overwriten
// 2. no checking. the db is overwriten

SQLITE_PRIVATE void on_request_state_update(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  void *header, *body, *signatures;
  int64 height, current_height;
  binn *map=NULL, *list=NULL;
  binn_iter iter;
  binn item;
  void *array=NULL;
  int rc;

  height = binn_map_int64(msg, PLUGIN_HEIGHT);

  if( plugin->current_block ){
    current_height = plugin->current_block->height;
    header = plugin->current_block->header;
    signatures = plugin->current_block->signatures;
  }else{
    current_height = 0;
    header = NULL;
    signatures = NULL;
  }

  SYNCTRACE("on_request_state_update - from height: %" INT64_FORMAT
            " current height: %" INT64_FORMAT "\n", height, current_height);

  if( height<0 ) height = 0;

  if( height>=current_height ){
    /* inform that it is up-to-date */
    SYNCTRACE("on_request_state_update - peer is up-to-date\n");
    map = binn_map();
    if( !map ) goto loc_failed;
    if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_UPTODATE)==FALSE ) goto loc_failed;
    if( current_height>0 ){
      if( binn_map_set_map(map, PLUGIN_STATE, header)==FALSE ) goto loc_failed;
      //if( binn_map_set_list(map, PLUGIN_SIGNATURES, signatures)==FALSE ) goto loc_failed;
    }
    if( send_peer_message(node, map, NULL)==FALSE ) goto loc_failed;
    binn_free(map); map = NULL;
    /* if there are open uncommitted blocks, send them to the peer */
    send_new_blocks(plugin, node);
    send_block_votes(plugin, node);
    return;
  }


  /* if there is an open verified block, roll it back */
  if( plugin->open_block ){
    rollback_block(plugin);
  }

  /* start reading the current state of the database */
  rc = aergolite_begin_state_read(this_node);
  if( rc ) goto loc_failed;

  /* get the list of modified pages since the informed height */
  rc = aergolite_get_modified_pages(this_node, height + 1, &list);
  if( rc ) goto loc_failed;
  assert(binn_count(list)>0);

  /* build an array with the distinct modified pages */
  array = new_array(16, sizeof(Pgno));
  if( !array ){ rc = SQLITE_NOMEM; goto loc_failed; }
  binn_list_foreach(list, item) {
    Pgno pgno = binn_list_uint32(item.ptr, 1);
    int pos = array_insert_sorted(&array, &pgno, compare_pgno, TRUE);
    if( pos<0 ){ rc = SQLITE_NOMEM; goto loc_failed; }
  }

  /* for each modified page */
  {
  int i;
  int count = array_count(array);
  Pgno *pages = (Pgno*) array_ptr(array);
  for( i=0; i<count; i++ ){
    Pgno pgno = pages[i];
    char data[4096];  //! should it support bigger page sizes?
    int size;
    SYNCTRACE("on_request_state_update - sending page %d...\n", pgno);
    /* read the page content */
    rc = aergolite_get_db_page(this_node, pgno, data, &size); // -- it can return a ptr
    if( rc ) goto loc_failed;
    /* send the page to the peer */
    map = binn_map();
    if( !map ) goto loc_failed;
    if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_DB_PAGE)==FALSE ) goto loc_failed;
    if( binn_map_set_uint32(map, PLUGIN_PGNO, pgno)==FALSE ) goto loc_failed;
    if( binn_map_set_blob(map, PLUGIN_DBPAGE, data, size)==FALSE ) goto loc_failed;
    if( binn_map_set_int64(map, PLUGIN_HEIGHT, current_height)==FALSE ) goto loc_failed;
    if( send_peer_message(node, map, NULL)==FALSE ) goto loc_failed;
    binn_free(map); map = NULL;
  }
  array_free(&array);
  }

  aergolite_end_state_read(this_node);
  //if( rc ) ...

  /* send the block header (state agreement) to the peer */
  SYNCTRACE("on_request_state_update - sending the state agreement\n");
  map = binn_map();
  if( !map ) goto loc_failed;
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_APPLY_UPDATE)==FALSE ) goto loc_failed;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, current_height)==FALSE ) goto loc_failed;
  if( binn_map_set_map(map, PLUGIN_STATE, header)==FALSE ) goto loc_failed;
  //if( binn_map_set_list(map, PLUGIN_SIGNATURES, signatures)==FALSE ) goto loc_failed;
  if( binn_map_set_list(map, PLUGIN_MOD_PAGES, list)==FALSE ) goto loc_failed;
  if( send_peer_message(node, map, NULL)==FALSE ) goto loc_failed;
  binn_free(map); map = NULL;

  binn_free(list); list = NULL;

  /* if there are open uncommitted blocks, send them to the peer */
  send_new_blocks(plugin, node);
  send_block_votes(plugin, node);

  return;
loc_failed:
  SYNCTRACE("on_request_state_update - FAILED\n");
  if( map ) binn_free(map);
  if( list ) binn_free(list);
  if( array ) array_free(&array);
  aergolite_end_state_read(this_node);
}
