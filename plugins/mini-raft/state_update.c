
#if 0

SQLITE_PRIVATE void on_transaction_request_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_transaction_request_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_requested_transaction_not_found(node *node, void *msg, int size) {
  int rc;

  /* the local transaction (from wal-remote or main db) is not present in the primary node */

  /* save the local db and download a new one from the primary node */
//  rc = aergolite_store_and_empty_local_db(node->this_node);

//!  if( rc ){
//    /* disconnect from the primary node */
//    disconnect_peer(this_node->leader_node);  //! ??  should it retry after an interval? (uv_timer)
//  }

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

SQLITE_PRIVATE void on_in_sync_message(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;

  /* update the state */
  plugin->sync_down_state = DB_STATE_IN_SYNC;

  /* start sending the local transactions */
  if( plugin->sync_up_state!=DB_STATE_SYNCHRONIZING && plugin->sync_up_state!=DB_STATE_IN_SYNC ){
    start_upstream_db_sync(plugin);
  }

}

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

/* update this node's db state with the peers' current state  (sync down) */
/* if the this node's current state is invalid or empty, download a new db from the peers */
SQLITE_PRIVATE void request_state_update(plugin *plugin) {
  int64 current_height;
  char *state_hash;
  binn *map;

  SYNCTRACE("request_state_update\n");

  if( !plugin->leader_node ){
    plugin->sync_down_state = DB_STATE_UNKNOWN;
    return;
  }

  plugin->sync_down_state = DB_STATE_SYNCHRONIZING;

  map = binn_map();
  if( !map ) goto loc_failed;

  if( plugin->current_block ){
    current_height = plugin->current_block->height;
    //state_hash = plugin->current_block->state_hash;
  }else{
    current_height = 0;
    //state_hash = NULL;
  }

  SYNCTRACE("request_state_update - current_height=%" INT64_FORMAT "\n", current_height);

  /* create request packet */
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_REQUEST_STATE_DIFF)==FALSE ) goto loc_failed;
  if( binn_map_set_int64(map, PLUGIN_HEIGHT, current_height)==FALSE ) goto loc_failed;
  //if( state_hash ){
  //  if( binn_map_set_blob(map, STATE_HASH, state_hash, SHA256_BLOCK_SIZE)==FALSE ) goto loc_failed;
  //}

  /* send the packet */
  if( send_peer_message(plugin->leader_node, map, on_state_update_request_sent)==FALSE ) goto loc_failed;

  binn_free(map);

  return;
loc_failed:
  if( map ) binn_free(map);
  plugin->sync_down_state = DB_STATE_ERROR;

// use a timer if it is off-line
// also call this fn when it reconnects

}

/****************************************************************************/

SQLITE_PRIVATE void on_update_db_page(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  unsigned int pgno;
  char *data;
  int rc;

  pgno = binn_map_uint32(msg, PLUGIN_PGNO);
  data = binn_map_blob(msg, PLUGIN_DBPAGE, &size);

  SYNCTRACE("on_update_db_page - pgno: %d  size: %d\n", pgno, size);

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

  return;

loc_failed:
  plugin->is_updating_state = false;
  plugin->sync_down_state = DB_STATE_ERROR;

}

/****************************************************************************/

SQLITE_PRIVATE void on_apply_state_update(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  void *header, *body, *signatures, *mod_pages;
  int64 height;
  struct block *block = NULL;
  int rc;

  height = binn_map_uint64(msg, PLUGIN_HEIGHT);
  header = binn_map_map(msg, PLUGIN_STATE);
//  body = binn_map_blob(msg, PLUGIN_PAYLOAD, &payload_size);
  signatures = binn_map_list(msg, PLUGIN_SIGNATURES);
  mod_pages = binn_map_list(msg, PLUGIN_MOD_PAGES);

  SYNCTRACE("on_apply_state_update - height: %" INT64_FORMAT
            " modified pages: %d\n", height, binn_count(mod_pages));

  assert(height>0);
  assert(header);
  //assert(signatures);
  assert(mod_pages);

  /* commit the new state */
  //rc = aergolite_apply_state_update(this_node, header, body, signatures);
  rc = aergolite_apply_state_update(this_node, header, signatures, mod_pages);
  if( rc ) goto loc_failed;

  /* keep the new state on the memory */
  block = sqlite3_malloc_zero(sizeof(struct block));
  if( !block ) goto loc_failed2;

  block->height = height;
  block->header = sqlite3_memdup(header, binn_size(header));
//  block->body = sqlite3_memdup(payload, payload_size);  // needed on full nodes?
  block->signatures = sqlite3_memdup(signatures, binn_size(signatures));

  if( !block->header ) goto loc_failed2;
//  if( !block->signatures ) goto loc_failed2;

  /* replace the previous block by the new one */
  discard_block(plugin->current_block);
  plugin->current_block = block;

  /* discard any new incoming block */
  discard_block(plugin->new_block);
  plugin->new_block = NULL;

  plugin->sync_down_state = DB_STATE_IN_SYNC;

  /* remove old transactions from mempool */
  check_mempool_transactions(plugin);

loc_exit:
  plugin->is_updating_state = false;
  return;

loc_failed:
  SYNCTRACE("on_apply_state_update - FAILED\n");
  plugin->sync_down_state = DB_STATE_ERROR;
  goto loc_exit;

loc_failed2:
  SYNCTRACE("on_apply_state_update - FAILED 2\n");
  /* force to reload the current state */
  discard_block(plugin->current_block);
  plugin->current_block = NULL;
  plugin->sync_down_state = DB_STATE_UNKNOWN;
  goto loc_exit;

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

  //if( height<0 ) ...
//!  if( height>current_height ) ...

  if( height==current_height ){
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
    return;
  }


  /* start reading the current state of the database */
  rc = aergolite_begin_state_read(this_node);
  if( rc ) goto loc_failed;

  /* get the list of modified pages since the informed height */
  rc = aergolite_get_modified_pages(this_node, height + 1, &list);
  if( rc ) goto loc_failed;

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

  return;
loc_failed:
  SYNCTRACE("on_request_state_update - FAILED\n");
  if( map ) binn_free(map);
  if( list ) binn_free(list);
  if( array ) array_free(&array);
  aergolite_end_state_read(this_node);

}
