/****************************************************************************/
/*** LEADER ELECTION ********************************************************/
/****************************************************************************/

SQLITE_PRIVATE void start_current_leader_query(plugin *plugin);
SQLITE_PRIVATE void on_leader_vote(plugin *plugin, node *node, char *arg);

struct leader_query {
  int node_id;
  bool answered;
};

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

SQLITE_PRIVATE BOOL has_nodes_for_query(plugin *plugin){
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

  SYNCTRACE("has_nodes_for_query connected=%d\n", count);

  if( count<2 ){
    return FALSE;
  }

  return TRUE;
}

/****************************************************************************/

SQLITE_PRIVATE void clear_leader_votes(plugin *plugin) {

  while( plugin->leader_votes ){
    struct leader_votes *next = plugin->leader_votes->next;
    sqlite3_free(plugin->leader_votes);
    plugin->leader_votes = next;
  }

}

/****************************************************************************/

SQLITE_PRIVATE void exit_election(plugin *plugin) {

  plugin->in_election = FALSE;
  clear_leader_votes(plugin);

  /* stop the election info timer */
  uv_timer_stop(&plugin->election_info_timer);

}

/****************************************************************************/

//! leader election: full nodes must have the preference, between those who have the last state

SQLITE_PRIVATE void new_leader_election(plugin *plugin) {
  node *node;

  SYNCTRACE("new_leader_election\n");

  if( plugin->in_election ) return;

  plugin->in_election = TRUE;
  plugin->in_leader_query = FALSE;
  array_free(&plugin->leader_query);

  plugin->leader_node = NULL;
  plugin->is_leader = FALSE;

  reset_node_state(plugin);

  clear_leader_votes(plugin);

  for( node=plugin->peers; node; node=node->next ){
    node->last_block = -1;
  }

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 5000, 0);

}

/****************************************************************************/

SQLITE_PRIVATE void start_leader_election(plugin *plugin) {

  send_tcp_broadcast(plugin, "election");

  /* as the broadcast does not include itself, call the function directly */
  on_new_election_request(plugin, NULL, NULL);

}

/****************************************************************************/

SQLITE_PRIVATE void on_leader_check_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;
  struct leader_votes *votes;
  struct node *node;
  int leader_id=0;

  SYNCTRACE("on_leader_check_timeout\n");

  if( !plugin->in_leader_query && !plugin->in_election ) return;

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( plugin->in_election ){
      if( votes->count >= majority(plugin->total_authorized_nodes) ){
        leader_id = votes->id;
        break;
      }
    }else{  /* querying the current leader */
      leader_id = votes->id;
      break;
    }
  }

  if( plugin->in_leader_query ){
    array_free(&plugin->leader_query);
    if( plugin->some_nodes_in_election ){
      SYNCTRACE("on_leader_check_timeout: some nodes in election\n");
      start_current_leader_query(plugin);
      return;
    }
    if( leader_id==0 ){
      SYNCTRACE("on_leader_check_timeout: no current leader\n");
      start_leader_election(plugin);
      return;
    }
  }

  if( plugin->in_election && leader_id==0 ){
#ifdef DEBUGPRINT
    int total = 0;
    for( votes=plugin->leader_votes; votes; votes=votes->next ){
      total += votes->count;
    }
    if( total<majority(plugin->total_authorized_nodes) ){
      SYNCTRACE("on_leader_check_timeout: no sufficient votes (total=%d required=%d)\n",
                total, majority(plugin->total_authorized_nodes));
    }else{
      SYNCTRACE("on_leader_check_timeout: no consensus on the current leader\n");
    }
#endif
    //start_leader_election(plugin);  -- it can start a new election within a timeout
    exit_election(plugin);
    start_current_leader_query(plugin);
    return;
  }


  if( plugin->in_election ){
    exit_election(plugin);
  }else{
    plugin->in_leader_query = FALSE;
  }


  /* check who is the current leader */
  if( leader_id==plugin->node_id ){
    plugin->is_leader = TRUE;
  }else{
    for( node=plugin->peers; node; node=node->next ){
      if( node->id==leader_id ){
        plugin->leader_node = node;
        break;
      }
    }
  }


  if( plugin->leader_node ){
    SYNCTRACE("on_leader_check_timeout: the current leader is at %s:%d with id %d\n",
              plugin->leader_node->host, plugin->leader_node->bind_port,
              plugin->leader_node->id);

    start_downstream_db_sync(plugin);

  }else if( plugin->is_leader ){
    SYNCTRACE("on_leader_check_timeout: this node is the current leader\n");

    plugin->sync_down_state = DB_STATE_IN_SYNC;

    //leader_node_process_local_transactions(plugin);

    if( plugin->mempool ){
      start_new_block_timer(plugin);
    }

  }else{
    SYNCTRACE("on_leader_check_timeout: not connected to the current leader\n");

    //check_peer_connection(ip_address, port);

    // maybe this is not required, as it is checking the connection to each
    // node that answers the broadcast request.

    start_node_discovery(plugin);

    reset_node_state(plugin);

    //plugin->leader_node = ...;  //! how to define it later?

  }


  /* activate a timer to retry the synchronization if it fails */
  SYNCTRACE("starting the process local transactions timer\n");
  uv_timer_start(&plugin->process_transactions_timer, process_transactions_timer_cb, 500, 500);

}

/****************************************************************************/

SQLITE_PRIVATE void start_current_leader_query(plugin *plugin) {
  node *node;
  int count;

  SYNCTRACE("start_current_leader_query\n");

  reset_node_state(plugin);

  if( !has_nodes_for_query(plugin) ){
    SYNCTRACE("start_current_leader_query - no sufficient nodes\n");
    return;
  }

  /* start a leader query */

  plugin->in_leader_query = TRUE;
  plugin->some_nodes_in_election = FALSE;
  plugin->leader_node = NULL;
  plugin->is_leader = FALSE;

  /* count how many authorized nodes are connected */
  count = 0;
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ) count++;
  }

  /* allocate array */
  plugin->leader_query = new_array(count, sizeof(struct leader_query));

  /* store the list of connected nodes */
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      struct leader_query queried_node;
      queried_node.node_id = node->id;
      queried_node.answered = false;
      array_append(&plugin->leader_query, &queried_node);
    }
  }

  clear_leader_votes(plugin);

  send_tcp_broadcast(plugin, "leader?");

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 1000, 0);

}

/****************************************************************************/

SQLITE_PRIVATE void check_current_leader(plugin *plugin) {

  if( !plugin->is_leader && !plugin->leader_node && !plugin->in_leader_query && !plugin->in_election ){
    start_current_leader_query(plugin);
  }

}

/****************************************************************************/

/*
** calculation: last_block_height xor node->id => biggest
** not including the current leader
*/
SQLITE_PRIVATE int calculate_new_leader(plugin *plugin){
  node *node;
  uint64 number, biggest=0, last_block, max_blocks=0;

  SYNCTRACE("calculate_new_leader\n");

  if( plugin->current_block ){
    last_block = plugin->current_block->height;
  }else{
    last_block = 0;
  }

  /* check the highest number of transactions */
  max_blocks = last_block;
  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized && node->last_block!=-1 ){
      if( node->last_block>max_blocks ) max_blocks = node->last_block;
    }
  }

  SYNCTRACE("calculate_new_leader max_blocks=%" INT64_FORMAT "\n", max_blocks);

  SYNCTRACE("calculate_new_leader this_node_id=%d last_block=%" INT64_FORMAT "\n", plugin->node_id, last_block);
  if( last_block==max_blocks ){
    number = max_blocks ^ (uint64)plugin->node_id;
    if( number>biggest ) biggest = number;
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      SYNCTRACE("calculate_new_leader node_id=%d last_block=%" INT64_FORMAT "\n", node->id, node->last_block);
      if( node->last_block==max_blocks && node!=plugin->last_leader ){
        number = max_blocks ^ (uint64)node->id;
        if( number>biggest ) biggest = number;
      }
    }
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->is_authorized ){
      if( node->last_block==max_blocks && node!=plugin->last_leader ){
        number = max_blocks ^ (uint64)node->id;
        if( number==biggest ) return node->id;
      }
    }
  }

  return plugin->node_id;
}

/****************************************************************************/

SQLITE_PRIVATE void broadcast_leader_vote(plugin *plugin){
  char message[32];
  int leader_id;

  /* xxx */

  if( !plugin->in_election ) return;

  leader_id = calculate_new_leader(plugin);

  SYNCTRACE("broadcast_leader_vote id=%d\n", leader_id);

  sprintf(message, "vote:%d", leader_id);

  send_tcp_broadcast(plugin, message);

  /* record this node's vote */

  on_leader_vote(plugin, NULL, message + 5);

}

/****************************************************************************/

SQLITE_PRIVATE void election_info_timeout(uv_timer_t* handle) {
  plugin *plugin = (struct plugin *) handle->loop->data;

  broadcast_leader_vote(plugin);

}

/****************************************************************************/
/*** TCP MESSAGES ***********************************************************/
/****************************************************************************/

/* "leader?": a broadcast message requesting the current leader */
SQLITE_PRIVATE void on_get_current_leader(
  plugin *plugin,
  node *node,
  char *arg
){
  char response[32];
  int leader_id;

  /* send a response message */

  if( plugin->is_leader ){
    leader_id = plugin->node_id;
  }else if( plugin->leader_node ){
    leader_id = plugin->leader_node->id;
  }else if( plugin->in_election ){
    leader_id = -1;
  }else{
    leader_id = 0;
  }

  SYNCTRACE("on_get_current_leader - leader=%d\n", leader_id);

  sprintf(response, "leader:%d", leader_id);
  send_text_message(node, response);

}

/****************************************************************************/

/* "election": a broadcast message requesting a leader election */
SQLITE_PRIVATE void on_new_election_request(
  plugin *plugin,
  node *node,
  char *arg
){
  int interval;

  if( plugin->in_election ) return;

  if( !has_nodes_for_consensus(plugin) ){
    SYNCTRACE("on_new_election_request - no sufficient nodes\n");
    if( node ){
      request_peer_list(plugin, node);
    }else{
      return;
    }
  }

  if( plugin->leader_node ){
    /* check if the current leader is still alive */
    send_text_message(plugin->leader_node, "ping");
    /* we cannot send new txns to the current leader until the election is done */
    plugin->last_leader = plugin->leader_node;
    plugin->leader_node = NULL;
  }else{
    plugin->last_leader = NULL;
  }

  new_leader_election(plugin);

  interval = 20 * plugin->total_authorized_nodes;
  if( interval<500 ) interval = 500;
  if( interval>3000 ) interval = 3000;
  uv_timer_start(&plugin->election_info_timer, election_info_timeout, interval, 0);

  {
    //send_broadcast_messagef(plugin, "last_block:%lld:%d", last_block, plugin->node_id);
    char message[64];
    int64 last_block = plugin->current_block ? plugin->current_block->height : 0;
    SYNCTRACE("this node's last block height: %" INT64_FORMAT "\n", last_block);
    sprintf(message, "last_block:%lld:%d", last_block, plugin->node_id);
    send_tcp_broadcast(plugin, message);
  }

}

/****************************************************************************/

/* "pong": a response message from the current leader */
SQLITE_PRIVATE void on_leader_ping_response(
  plugin *plugin,
  node *node,
  char *arg
){

  /* xxx */

  if( plugin->leader_node==NULL && plugin->last_leader ){   //! what if the leader is not connected via TCP?
    char message[32];
    SYNCTRACE("the current leader answered\n");
    sprintf(message, "vote:%d", plugin->last_leader->id);
    send_tcp_broadcast(plugin, message);
    uv_timer_stop(&plugin->election_info_timer);
  }

}

/****************************************************************************/

/* "last_block": a response message informing the height of the last block on the peer's blockchain */
SQLITE_PRIVATE void on_peer_last_block(
  plugin *plugin,
  node *node,
  char *arg
){
  int node_id=0;
  int last_block=0;
  char *pid, *pnum;

  if( !plugin->in_election ) return;

  pnum = arg;
  pid = stripchr(pnum, ':');
  last_block = atoi(pnum);  //! change to unsigned 64 bit
  node_id = atoi(pid);

  SYNCTRACE("node %d last block height: %d\n", node_id, last_block);

  node->last_block = last_block;

  for( node=plugin->peers; node; node=node->next ){
    /* if some node did not send the election info yet */
    if( node->is_authorized && node->last_block==-1 ) return;
  }

  /* all nodes answered. proceed with the election */
  election_info_timeout(&plugin->election_info_timer);
  uv_timer_stop(&plugin->election_info_timer);

}

/****************************************************************************/

/* "leader": a response message informing the peer leader */
SQLITE_PRIVATE void on_requested_peer_leader(
  plugin *plugin,
  node *node,
  char *arg
){
  struct leader_votes *votes = NULL;
  int leader_id, total_votes, i, count;

  if( !plugin->in_leader_query ) return;
  /* if an election was started in the middle of the inquiry, ignore the message */
  if( plugin->in_election ) return;

  leader_id = atoi(arg);

  /* mark that the contacted node has answered */
  count = array_count(plugin->leader_query);
  for( i=0; i<count; i++ ){
    struct leader_query *queried_node;
    queried_node = array_get(plugin->leader_query, i);
    if( queried_node->node_id==node->id ){
      queried_node->answered = true;
      goto loc_found;
    }
  }

  /* the node is not in the list of contacted nodes */
  return;

loc_found:

  /* if an election is taking place on other nodes, make a new leader query later */
  if( leader_id==-1 ){
    plugin->some_nodes_in_election = TRUE;
    goto loc_exit;
  }

  /* count the number of answers */
  total_votes = 0;
  count = array_count(plugin->leader_query);
  for( i=0; i<count; i++ ){
    struct leader_query *queried_node;
    queried_node = array_get(plugin->leader_query, i);
    if( queried_node->answered ){
      total_votes++;
    }
  }

  if( leader_id==0 ) goto loc_exit;

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->id==leader_id ){
      votes->count++;
      break;
    }
  }

  if( !votes ){  /* no item allocated for the given node id */
    if( plugin->leader_votes && plugin->leader_votes->id!=0 && leader_id!=0 ){
      /* there are nodes with different leader */
      SYNCTRACE("on_requested_peer_leader: some node(s) with a different leader."
                " leader1=%d leader2=%d\n", plugin->leader_votes->id, leader_id);
      start_leader_election(plugin);
      return;
    }
    /* store the node id */
    votes = sqlite3_malloc(sizeof(struct leader_votes));
    if( !votes ) return;
    votes->id = leader_id;
    votes->count = 1;
    /* add it to the list */
    votes->next = plugin->leader_votes;
    plugin->leader_votes = votes;
  }

  assert( plugin->total_authorized_nodes>1 );

loc_exit:

  /* check if the number of votes for a single node reaches the majority
  ** or if all the contacted nodes answered */
  if( (votes && votes->count >= majority(plugin->total_authorized_nodes)-1) ||
      total_votes+1==plugin->total_authorized_nodes ||
      total_votes==array_count(plugin->leader_query) ){
    /* stop the query timer */
    uv_timer_stop(&plugin->leader_check_timer);
    on_leader_check_timeout(&plugin->leader_check_timer);
  }

}

/****************************************************************************/

/* "vote": a response message informing the leader vote */
SQLITE_PRIVATE void on_leader_vote(
  plugin *plugin,
  node *node,
  char *arg
){
  struct leader_votes *votes;
  int leader_id, total_votes;

  /* if this node is not in an election, ignore the message */
  if( !plugin->in_election ) return;

  leader_id = atoi(arg);

  total_votes = 1;  /* the arriving 'vote' */
  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    total_votes += votes->count;
  }

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->id==leader_id ){
      votes->count++;
      break;
    }
  }

  if( !votes ){  /* no item allocated for the given node id */
    votes = sqlite3_malloc(sizeof(struct leader_votes));
    if( !votes ) return;
    votes->id = leader_id;
    votes->count = 1;
    /* add it to the list */
    votes->next = plugin->leader_votes;
    plugin->leader_votes = votes;
  }

  assert( plugin->total_authorized_nodes>1 );

  /* stop the election timer if the number of votes for a single node reaches the majority */
  if( votes->count >= majority(plugin->total_authorized_nodes) ||
      total_votes==plugin->total_authorized_nodes ){
    uv_timer_stop(&plugin->leader_check_timer);
    on_leader_check_timeout(&plugin->leader_check_timer);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void leader_election_init(){

  /* a broadcast message requesting the current leader */
  register_tcp_message("leader?", on_get_current_leader);
  /* a response message informing the peer leader */
  register_tcp_message("leader", on_requested_peer_leader);

  /* a broadcast message requesting a leader election */
  register_tcp_message("election", on_new_election_request);
  /* a response message informing the height of the last block on the peer's blockchain */
  register_tcp_message("last_block", on_peer_last_block);
  /* a response message informing the leader vote */
  register_tcp_message("vote", on_leader_vote);
  /* a response message from the current leader */
  register_tcp_message("pong", on_leader_ping_response);

}
