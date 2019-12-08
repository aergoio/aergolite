/****************************************************************************/
/*** ALLOWED NODES **********************************************************/
/****************************************************************************/

/*
** Updates the number of allowed nodes on this network.
** It must also count the nodes that are off-line
*/
SQLITE_PRIVATE void count_authorized_nodes(plugin *plugin) {

  plugin->total_authorized_nodes = aergolite_num_authorized_nodes(plugin->this_node);

  SYNCTRACE("count_authorized_nodes total_authorized_nodes=%d\n", plugin->total_authorized_nodes);

}

/****************************************************************************/

SQLITE_PRIVATE int send_auth_cb(
  void *arg,
  int   node_id,
  char *pubkey,
  int   pklen,
  void *log,
  int64 last_nonce
){
  binn *list = (binn*) arg;
  if( binn_list_add_list(list,log)==FALSE ) return SQLITE_NOMEM;
  return SQLITE_OK;
}

SQLITE_PRIVATE int send_authorizations(node *node, void *log){
  plugin *plugin = node->plugin;
  aergolite *this_node = plugin->this_node;
  binn *list, *map=NULL;
  int rc;

  SYNCTRACE("send_authorizations log=%p\n", log);

  if( !node->is_authorized ) return SQLITE_PERM;

  /* send all the authorizations to the remote node */

  list = binn_list();

  if( log ){
    /* add the given authorization to the list */
    if( binn_list_add_list(list, log)==FALSE ) goto loc_failed;
  }else{
    /* add all authorizations to the list */
    rc = aergolite_iterate_authorizations(this_node, send_auth_cb, list);
    if( rc ) goto loc_failed;
  }

  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_AUTHORIZATION)==FALSE ) goto loc_failed;
  if( binn_map_set_list(map, PLUGIN_AUTHORIZATION, list)==FALSE ) goto loc_failed;

  if( send_peer_message(node, map, NULL)==FALSE ){
    sqlite3_log(1, "send_node_identification: send_peer_message failed");
    goto loc_failed;
  }

  node->authorization_sent = TRUE;

  rc = SQLITE_OK;

loc_exit:
  binn_free(map);
  binn_free(list);
  return rc;

loc_failed:
  sqlite3_log(1, "send_node_identification FAILED");
  rc = SQLITE_NOMEM;
  goto loc_exit;

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_accepted_node(node *node) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_new_accepted_node\n");

  send_peer_list(plugin, node);

  send_mempool_transactions(plugin, node);

  check_current_leader(plugin);

}

/****************************************************************************/

SQLITE_PRIVATE int on_new_authorization(plugin *plugin, void *log, BOOL from_network){
  aergolite *this_node = plugin->this_node;
  node *node, *authorized_node=NULL;
  char pubkey[36];
  int rc, pklen;

  SYNCTRACE("on_new_authorization\n");

  /* verify and store the authorization */
  rc = aergolite_new_authorization(this_node, log, pubkey, &pklen);
  if( rc ) return rc;

  /* update the number of authorized nodes */
  count_authorized_nodes(plugin);

  if( plugin->pklen==pklen && memcmp(plugin->pubkey,pubkey,pklen)==0 ){
    plugin->is_authorized = TRUE;
  }

  for( node=plugin->peers; node; node=node->next ){
    if( node->pklen==pklen && memcmp(node->pubkey,pubkey,pklen)==0 ){
      node->is_authorized = TRUE;
      authorized_node = node;
      if( !from_network ){
        /* send all the authorizations to the new node */
        rc = send_authorizations(node, NULL);
      }
    }else if( node->is_authorized && !from_network ){
      /* send only this new authorization to this node */
      rc = send_authorizations(node, log);
    }
    if( rc ) break;
  }

  /* if the authorized node is online */
  if( authorized_node ){
    on_new_accepted_node(authorized_node);
  }

  return rc;
}

/****************************************************************************/

SQLITE_PRIVATE void on_authorization_received(node *node, void *msg, int size){
  plugin *plugin = node->plugin;
  binn_iter iter;
  binn item;
  void *list;

  list = binn_map_list(msg, PLUGIN_AUTHORIZATION);

  SYNCTRACE("authorizations received - count=%d\n", binn_count(list));

  if( !list ){
    sqlite3_log(1, "on_authorization_received: invalid packet");
    return;
  }

  binn_list_foreach(list, item){
    assert( item.type==BINN_LIST );
    void *log = item.ptr;
    on_new_authorization(plugin, log, TRUE);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_valid_node_id(node *node) {
  plugin *plugin = node->plugin;
  int rc;

  SYNCTRACE("on_valid_node_id\n");

  if( !node->is_authorized ) return;

  /* send all the authorizations to the remote node */
  rc = send_authorizations(node, NULL);

  on_new_accepted_node(node);

}

/****************************************************************************/
/* NODE ID CONFLICT *********************************************************/
/****************************************************************************/

SQLITE_PRIVATE void stop_id_conflict_timer(struct node_id_conflict *id_conflict) {

  id_conflict->existing_node->id_conflict = NULL;
  id_conflict->new_node->id_conflict = NULL;

  uv_timer_stop(&id_conflict->timer);
  uv_close2( (uv_handle_t*) &id_conflict->timer, worker_thread_on_close);

}

SQLITE_PRIVATE void id_conflict_timer_cb(uv_timer_t* handle) {
  //aergolite *this_node = (aergolite *) handle->loop->data;
  struct node_id_conflict *id_conflict = (struct node_id_conflict *) handle->data;
  node *existing_node, *new_node;

  // 5. if the timer is fired before the answer:
  //        1. close the old connection
  //        2. continue with the identification process with the new node

  existing_node = id_conflict->existing_node;
  new_node = id_conflict->new_node;

  stop_id_conflict_timer(id_conflict);

  disconnect_peer(existing_node);

  /* start the db sync */
  on_valid_node_id(new_node);

}

SQLITE_PRIVATE void on_id_conflict_recvd(node *node, void *msg, int size) {

  sqlite3_log(1, "this node has the same node id as another one");

}

SQLITE_PRIVATE void on_id_conflict_sent(send_message_t *req, int status) {

  /* disconnect the peer */
  uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);

}

SQLITE_PRIVATE void on_ping_received(node *node, void *msg, int size) {
  binn *map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_PONG) == FALSE ||
      send_peer_message(node, map, NULL) == FALSE )
  {
    sqlite3_log(1, "on_ping_received: send_peer_message failed");
  }
  if (map) binn_free(map);
}

SQLITE_PRIVATE void on_ping_response(node *node, void *msg, int size) {
  struct node_id_conflict *id_conflict;
  binn *map;

  id_conflict = node->id_conflict;
  if( !id_conflict ) return;

  /* send a message to the new node informing about the conflict */
  map = binn_map();
  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_ID_CONFLICT) == FALSE ||
      send_peer_message(id_conflict->new_node, map, on_id_conflict_sent) == FALSE )
  {
    sqlite3_log(1, "on_ping_response: send_peer_message failed");
  }
  if (map) binn_free(map);

  /* stop the timer */
  stop_id_conflict_timer(id_conflict);

}

SQLITE_PRIVATE void on_new_node_with_same_id(node *existing_node, node *new_node) {
  plugin *plugin = existing_node->plugin;
  aergolite *this_node = existing_node->this_node;
  struct node_id_conflict *id_conflict;
  binn *map;

  sqlite3_log(1, "new connected node has the same node id as another one");

  // 1. send a packet to the already connected node
  // 2. save info on a struct
  // 3. start a timer and pass this struct
  // 4. if the node answers before the timer:
  //        1. stop the timer
  //        2. send a message to the new connection informing about the colision
  //        3. close the new connection when the packet is sent
  // 5. if the timer is fired before the answer:
  //        1. close the old connection
  //        2. continue with the identification process with the new node

  // or send the msg to all the nodes with this id?

  //! this implementation is not perfect
  //  if 2 new nodes connect with the same node_id, the behavior is undefined

  /* send a packet to the already connected node */
  map = binn_map();
  if (binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_PING) == FALSE) goto loc_failed1;
  if (send_peer_message(existing_node, map, NULL) == FALSE) goto loc_failed1;
  binn_free(map); map = 0;

  /* save the conflict information */
  id_conflict = sqlite3_malloc_zero(sizeof(struct node_id_conflict));
  if( !id_conflict ) goto loc_failed2;
  id_conflict->existing_node = existing_node;
  id_conflict->new_node = new_node;

  existing_node->id_conflict = id_conflict;
  new_node->id_conflict = id_conflict;
//--
  //id_conflict->next = plugin->node_id_conflicts;
  //plugin->node_id_conflicts = id_conflict;
//++

  /* initialize the node id conflict timer */
  SYNCTRACE("starting the id conflict timer\n");
  uv_timer_init(plugin->loop, &id_conflict->timer);
  id_conflict->timer.data = id_conflict;  /* release on timer close */
  uv_timer_start(&id_conflict->timer, id_conflict_timer_cb, 3000, 0);  /* wait 3 seconds */

  return;
loc_failed1:
  sqlite3_log(1, "on_new_node_with_same_id: send_peer_message failed");
  if (map) binn_free(map);
loc_failed2:
  disconnect_peer(existing_node);

}

/****************************************************************************/

SQLITE_PRIVATE void check_new_identified_node(node *new_node) {
  plugin *plugin = new_node->plugin;
  aergolite *this_node = new_node->this_node;
  struct node *node;

  if( memcmp(new_node->pubkey,plugin->pubkey,33)==0 ){
    SYNCTRACE("connection from this same node!!!\n");
    goto loc_invalid_peer;
  }

  /* check for node id conflict with this node */
  if( new_node->id==plugin->node_id ){
    sqlite3_log(1, "new connected node has the same node id as this node");
    goto loc_invalid_peer;
  }

  /* check if this node has a previous connection */
  for( node=plugin->peers; node; node=node->next ){
    if( node!=new_node && memcmp(node->pubkey,new_node->pubkey,33)==0 ){
      SYNCTRACE("discarding previous connection from remote node\n");
      disconnect_peer(node);
      return;
    }
  }

  /* check for node id conflict with other node */
  for( node=plugin->peers; node; node=node->next ){
    if( node!=new_node && node->id==new_node->id ){
      on_new_node_with_same_id(node, new_node);
      return;
    }
  }

  /* start the db sync */
  on_valid_node_id(new_node);

  return;

loc_invalid_peer:
  /* disconnect the peer */
  disconnect_peer(new_node);

}

/****************************************************************************/

#if 0

SQLITE_PRIVATE void on_new_id_request_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_new_id_request_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_id_received(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int node_id;

  node_id = binn_map_int32(msg, PLUGIN_NODE_ID);

  if( node_id > 0 ){
    /* save the node id in the local config */
    int rc = aergolite_set_node_config_int(this_node, "node_id", node_id);
    if( rc ){
      sqlite3_log(rc, "on_new_node_id_received: could not save the new node id in the config");
      /* disconnect the peer */
      disconnect_peer(node);
      return;
    }
    /* store it in the struct */
    plugin->node_id = node_id;
    /* check if valid and start db sync */
    check_new_node_id(node);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_new_node_id_request(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  int  rc, node_id, max_node_id=0;
  binn *map=0;

  /* get a new node id */
  node_id = aergolite_get_node_config_int(this_node, "last_node_id");
  if( node_id > 1 ){
    node_id++;
  } else {
    /* get an unused node id */
    struct node *tnode;
    for (tnode = plugin->peers; tnode; tnode = tnode->next) {
      if (tnode->id > max_node_id) max_node_id = tnode->id;
    }
    if( max_node_id==0 ) max_node_id = 1;
    node_id = max_node_id + 1;
  }
  if( node_id==plugin->node_id ) node_id++;
  rc = aergolite_set_node_config_int(this_node, "last_node_id", node_id);
  if( rc ){
    sqlite3_log(rc, "on_new_node_id_request: could not save the new node id in the config");
    goto loc_failed;
  }

  /* send it to the secondary node */
  map = binn_map();
  if (binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_NEW_NODE_ID) == FALSE) goto loc_failed;
  if (binn_map_set_int32(map, PLUGIN_NODE_ID, node_id) == FALSE) goto loc_failed;
  if (send_peer_message(node, map, on_new_id_request_sent) == FALSE) {
    sqlite3_log(1, "on_new_node_id_request: send_peer_message failed");
loc_failed:
    if (map) binn_free(map);
    disconnect_peer(node);
    return;
  }

  /* save the node id in the node struct */
  node->id = node_id;

  check_new_node_id(node);

}

#endif

/****************************************************************************/

SQLITE_PRIVATE void on_node_identification(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  aergolite *this_node = node->this_node;
  char *signature, pubkey_int[96], *pubkey_ext, msg2[2048];
  char *cpu, *os, *host, *app, *pubkey;
  int rc, version, siglen=0, pklen, pklen_int=0, pklen_ext=0;

  version = binn_map_int32(msg, PLUGIN_VERSION);
  if( version!=PLUGIN_VERSION_NUMBER ){
    sqlite3_log(1, "on_node_identification: wrong protocol version: %d", version);
    disconnect_peer(node);
    return;
  }

  node->id = binn_map_int32(msg, PLUGIN_NODE_ID);
  node->info = binn_map_str(msg, PLUGIN_NODE_INFO);
  node->bind_port = binn_map_int32(msg, PLUGIN_PORT);
  cpu = binn_map_str(msg, PLUGIN_CPU);
  os = binn_map_str(msg, PLUGIN_OS);
  host = binn_map_str(msg, PLUGIN_HOSTNAME);
  app = binn_map_str(msg, PLUGIN_APP);
  pubkey_ext = binn_map_blob(msg, PLUGIN_PUBKEY, &pklen_ext);
  signature = binn_map_blob(msg, PLUGIN_SIGNATURE, &siglen);

  SYNCTRACE("on_node_identification - node_id=%d remote_bind_port=%d remote_port=%d\n",
            node->id, node->bind_port, node->port);
  SYNCTRACE("on_node_identification - hostname=[%s] app=[%s]\n", host, app);

  if( node->info ){
    node->info = sqlite3_strdup(node->info);
  }

  /* if the node id was not supplied */
  if( plugin->node_id==0 ){
    sqlite3_log(1, "on_node_identification: empty node id!");
    goto loc_failed;
  }else if( node->id < 0 ){
    sqlite3_log(1, "on_node_identification: invalid node id!");
    goto loc_failed;
  }else if( !signature ){
    sqlite3_log(1, "on_node_identification: no digital signature");
    goto loc_failed;
  }

  /* load the public key from the allowed nodes table */
  rc = aergolite_get_authorization(this_node, node->id, pubkey_int, &pklen_int, NULL, NULL);
  if( rc==SQLITE_OK ){
    if( pubkey_ext ){
      if( pklen_int!=pklen_ext || memcmp(pubkey_int, pubkey_ext, pklen_int)!=0 ){
        sqlite3_log(1, "on_node_identification: different public keys for node %d", node->id);
        goto loc_failed;
      }
    }
    pubkey = pubkey_int;
    pklen = pklen_int;
  }else{
    pubkey = pubkey_ext;
    pklen = pklen_ext;
    assert(pklen_int==0);
  }

  if( pklen_int==0 && pklen_ext==0 ){
    sqlite3_log(1, "on_node_identification: no public key");
    goto loc_failed;
  }

  /* verify the digital signature */
  sprintf(msg2, "%d:%d:%s:%s:%s:%s", node->id, node->bind_port, cpu, os, host, app);
  rc = aergolite_verify(this_node, msg2, strlen(msg2), pubkey, pklen, signature, siglen);
  if( rc==SQLITE_INVALID ){
    sqlite3_log(rc, "on_node_identification: invalid digital signature");
    goto loc_failed;
  }else if( rc ){
    sqlite3_log(rc, "on_node_identification: digital signature failed");
    goto loc_failed;
  }

  SYNCTRACE("remote node authenticated - node_id=%d\n", node->id);

  /* store the node info */
  strcpy(node->cpu, cpu);
  strcpy(node->os, os);
  strcpy(node->hostname, host);
  strcpy(node->app, app);
  memcpy(node->pubkey, pubkey, pklen);
  node->pklen = pklen;

  /* store the node state */
  if( pklen_int>0 ){
    /* authorization stored on the blockchain */
    node->is_authorized = TRUE;
  }else{
    /* check for authorization */
    rc = is_node_authorized(this_node, pubkey, pklen, &node->is_authorized);
    //if( rc ) xxx
  }

  if( pklen_ext==0 ){
    node->authorization_sent = TRUE;
  }

  /* xxx */
  check_new_identified_node(node);


//   3 possible cases:
// 1. both nodes have no content - they are just starting
// 2. one is part of the network and the other is not - this one can be any of these 2
// 3. both are already part of the network


  return;
loc_failed:
  disconnect_peer(node);
}

/****************************************************************************/

SQLITE_PRIVATE void on_id_msg_sent(send_message_t *req, int status) {

  if (status < 0) {
    SYNCTRACE("on_id_msg_sent FAILED - (%d) %s\n", status, uv_strerror(status));
    uv_close2( (uv_handle_t*) ((uv_write_t*)req)->handle, worker_thread_on_close);  /* disconnect */
  }

}

/****************************************************************************/

SQLITE_PRIVATE BOOL send_node_identification(plugin *plugin, node *node) {
  aergolite *this_node = plugin->this_node;
  char hostname[256], cpu_info[256], os_info[256], app_info[256], *node_info;
  char msg[2048], signature[72];
  binn *map;
  BOOL ret=FALSE;
  int rc, siglen;

  /* send the identification to the other peer */

  map = binn_map();
  if( !map ) goto loc_exit;

  if( binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_CMD_ID)==FALSE ) goto loc_failed;
  if( binn_map_set_int32(map, PLUGIN_VERSION, PLUGIN_VERSION_NUMBER)==FALSE ) goto loc_failed;
  if( binn_map_set_blob (map, PLUGIN_PUBKEY, plugin->pubkey, plugin->pklen)==FALSE ) goto loc_failed;
  if( binn_map_set_int32(map, PLUGIN_NODE_ID, plugin->node_id)==FALSE ) goto loc_failed;
  if( binn_map_set_int32(map, PLUGIN_PORT, plugin->bind->port)==FALSE ) goto loc_failed;

  get_this_device_info(hostname, cpu_info, os_info, app_info);

  if( binn_map_set_str(map, PLUGIN_CPU, cpu_info)==FALSE ) goto loc_failed;
  if( binn_map_set_str(map, PLUGIN_OS, os_info)==FALSE ) goto loc_failed;
  if( binn_map_set_str(map, PLUGIN_HOSTNAME, hostname)==FALSE ) goto loc_failed;
  if( binn_map_set_str(map, PLUGIN_APP, app_info)==FALSE ) goto loc_failed;

  node_info = aergolite_get_node_info(this_node);
  if( node_info ){
    if (binn_map_set_str(map, PLUGIN_NODE_INFO, node_info) == FALSE) goto loc_failed;
  }

  /* sign the message content */

  sprintf(msg, "%d:%d:%s:%s:%s:%s", plugin->node_id, plugin->bind->port,
          cpu_info, os_info, hostname, app_info);

  rc = aergolite_sign(this_node, msg, strlen(msg), signature, &siglen);
  if( rc ){
    sqlite3_log(rc, "send_node_identification: digital signature failed");
    goto loc_exit;
  }
  if( binn_map_set_blob(map, PLUGIN_SIGNATURE, signature, siglen)==FALSE ) goto loc_failed;

  /* send the message to the peer */

  if (send_peer_message(node, map, on_id_msg_sent) == FALSE) {
    sqlite3_log(1, "send_node_identification: send_peer_message failed");
    goto loc_exit;
  }

  ret = TRUE;

loc_exit:
  if( map ) binn_free(map);
  return ret;

loc_failed:
  sqlite3_log(1, "send_node_identification: binn failed. probably out of memory");
  goto loc_exit;
}
