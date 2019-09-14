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

SQLITE_PRIVATE void exit_election(plugin *plugin) {

  plugin->in_election = FALSE;
  clear_leader_votes(plugin);

}

/****************************************************************************/

//! leader election: full nodes must have the preference, between those who have the last state

SQLITE_PRIVATE void new_leader_election(plugin *plugin) {

  SYNCTRACE("new_leader_election\n");

  if( plugin->in_election ) return;
  plugin->in_election = TRUE;
  plugin->in_leader_query = FALSE;

  plugin->leader_node = NULL;
  plugin->is_leader = FALSE;

  reset_node_state(plugin);

  update_known_nodes(plugin);

  //! what if a (s)election is already taking place?
  clear_leader_votes(plugin);

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 3000, 0);

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

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->count >= majority(plugin->total_known_nodes) ){
      leader_id = votes->id;
      break;
    }
  }

  if( !plugin->in_election ){  /* inquirying the current leader */
    if( leader_id==0 ){
      SYNCTRACE("on_leader_check_timeout: no current leader\n");
      start_leader_election(plugin);
      return;
    }else{
      /* check if some node has a different leader */
      for( votes=plugin->leader_votes; votes; votes=votes->next ){
        if( votes->id!=leader_id && votes->id!=0 ){
          SYNCTRACE("on_leader_check_timeout: some node(s) with a different leader\n");
          /* start a leader election to make all nodes have the same leader */
          start_leader_election(plugin);
          return;
        }
      }
    }
  }

  if( plugin->in_election && leader_id==0 ){
#ifdef DEBUGPRINT
    int total = 0;
    for( votes=plugin->leader_votes; votes; votes=votes->next ){
      total += votes->count;
    }
    if( total<majority(plugin->total_known_nodes) ){
      SYNCTRACE("on_leader_check_timeout: no sufficient votes (total=%d required=%d)\n",
                total, majority(plugin->total_known_nodes));
    }else{
      SYNCTRACE("on_leader_check_timeout: no consensus on the current leader\n");
    }
#endif
    //start_leader_election(plugin);  -- it can start a new election within a timeout
    exit_election(plugin);
    check_current_leader(plugin);
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

SQLITE_PRIVATE void check_current_leader(plugin *plugin) {
  node *node;
  int count;

  reset_node_state(plugin);

  update_known_nodes(plugin);

  count = 1;  /* this node */
  for( node=plugin->peers; node; node=node->next ){
    if( node->id!=0 ) count++;
  }
  if( count<majority(plugin->total_known_nodes) ) return;

  /* start a leader query */

  plugin->in_leader_query = TRUE;
  plugin->leader_node = NULL;
  plugin->is_leader = FALSE;

  clear_leader_votes(plugin);

  send_tcp_broadcast(plugin, "leader?");

  uv_timer_start(&plugin->leader_check_timer, on_leader_check_timeout, 5000, 0);

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
    if( node->last_block>max_blocks ) max_blocks = node->last_block;
  }

  SYNCTRACE("calculate_new_leader max_blocks=%" INT64_FORMAT "\n", max_blocks);

  SYNCTRACE("calculate_new_leader node_id=%d\n", plugin->node_id);
  if( last_block==max_blocks ){
    number = max_blocks ^ (uint64)plugin->node_id;
    if( number>biggest ) biggest = number;
  }

  for( node=plugin->peers; node; node=node->next ){
    SYNCTRACE("calculate_new_leader node_id=%d last_block=%" INT64_FORMAT "\n", node->id, node->last_block);
    if( node->last_block==max_blocks && node!=plugin->last_leader ){
      number = max_blocks ^ (uint64)node->id;
      if( number>biggest ) biggest = number;
    }
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->last_block==max_blocks && node!=plugin->last_leader ){
      number = max_blocks ^ (uint64)node->id;
      if( number==biggest ) return node->id;
    }
  }

  return plugin->node_id;
}

/****************************************************************************/

SQLITE_PRIVATE void broadcast_leader_vote(plugin *plugin){
  char message[32];
  int leader_id;

  /* xxx */

  leader_id = calculate_new_leader(plugin);

  SYNCTRACE("broadcast_leader_vote id=%d\n", leader_id);

  sprintf(message, "vote:%d", leader_id);

  send_tcp_broadcast(plugin, message);

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

  if( plugin->in_election ) return;

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
  uv_timer_start(&plugin->election_info_timer, election_info_timeout, 1000, 0);

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

  //check_peer_connection(plugin, sender, get_sockaddr_port(addr));

  pnum = arg;
  pid = stripchr(pnum, ':');
  last_block = atoi(pnum);
  node_id = atoi(pid);

  SYNCTRACE("node %d last block height: %d\n", node_id, last_block);

  node->last_block = last_block;

}

/****************************************************************************/

/* "leader": a response message informing the peer leader */
SQLITE_PRIVATE void on_requested_peer_leader(
  plugin *plugin,
  node *node,
  char *arg
){
  struct leader_votes *votes;
  int node_id=0;

  if( !plugin->in_leader_query ) return;
  /* if an election was started in the middle of the inquiry, ignore the message */
  if( plugin->in_election ) return;

  node_id = atoi(arg);

  /* if an election is taking place on other nodes, try to participate */
  if( node_id==-1 ){
    on_new_election_request(plugin, NULL, NULL);
    return;
  }

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->id==node_id ){
      votes->count++;
      break;
    }
  }

  if( !votes ){  /* no item allocated for the given node id */
    votes = sqlite3_malloc(sizeof(struct leader_votes));
    if( !votes ) return;
    votes->id = node_id;
    votes->count = 1;
    /* add it to the list */
    votes->next = plugin->leader_votes;
    plugin->leader_votes = votes;
  }

  /* stop the election timer if the number of votes for a single node reaches the majority */
  if( plugin->total_known_nodes>1 && votes->count >= majority(plugin->total_known_nodes) ){
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
  int node_id=0;

  /* if this node is not in an election, ignore the message */
  if( !plugin->in_election ) return;

  node_id = atoi(arg);

  for( votes=plugin->leader_votes; votes; votes=votes->next ){
    if( votes->id==node_id ){
      votes->count++;
      break;
    }
  }

  if( !votes ){  /* no item allocated for the given node id */
    votes = sqlite3_malloc(sizeof(struct leader_votes));
    if( !votes ) return;
    votes->id = node_id;
    votes->count = 1;
    /* add it to the list */
    votes->next = plugin->leader_votes;
    plugin->leader_votes = votes;
  }

  /* stop the election timer if the number of votes for a single node reaches the majority */
  if( plugin->total_known_nodes>1 && votes->count >= majority(plugin->total_known_nodes) ){
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
