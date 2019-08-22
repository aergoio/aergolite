/****************************************************************************/
/*** LEADER ELECTION ********************************************************/
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
