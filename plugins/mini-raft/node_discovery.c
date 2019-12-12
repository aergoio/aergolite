/*

node discovery URI parameters:

discovery=local:4329  (local UDP broadcast - all nodes use the same port)

discovery=ip1:port1  (known node)

discovery=server:port:type  (server with list of peers - not implemented)

discovery=local:4329,ip1:port1,ip2:port2  (mixed)

*/

/****************************************************************************/

/* "whr?": a broadcast message to find peers */
SQLITE_PRIVATE void on_find_node_request(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *sender,
  char *sender_ip,
  char *arg
){
  uv_udp_send_t *send_req;
  uv_buf_t response = { .base="here", .len=5 };

  if( is_local_ip_address(sender_ip) ) return;

  /* send a response message */

  SYNCTRACE("on_udp_message: send a response message to %s\n", sender_ip);

  send_req = sqlite3_malloc(sizeof(uv_udp_send_t));
  if( !send_req ) return;

  uv_udp_send(send_req, socket, &response, 1, sender, on_udp_send);

}

/****************************************************************************/

/* "here": a response message informing a peer location */
SQLITE_PRIVATE void on_find_node_response(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *sender,
  char *sender_ip,
  char *arg
){

  check_peer_connection(plugin, sender_ip, get_sockaddr_port(sender));

  if( plugin->reconnect_timer_enabled ){
    /* disable the reconnection timer */
    uv_timer_stop(&plugin->reconnect_timer);
    plugin->reconnect_timer_enabled = 0;
    /* enable the after connections timer */
    uv_timer_start(&plugin->after_connections_timer, after_connections_timer_cb, 500, 0);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void on_peer_list_received(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;
  binn_iter iter;
  binn *list, item;
  BOOL is_connecting=FALSE;

  list = binn_map_list(msg, PLUGIN_PEERS);

  SYNCTRACE("peer list received - count=%d\n", binn_count(list));

  binn_list_foreach(list, item){
    char *host = binn_list_str(&item, 1);
    int   port = binn_list_int32(&item, 2);
    SYNCTRACE("\t node at %s:%d\n", host, port);
    if( check_peer_connection(plugin,host,port)==TRUE ){
      is_connecting = TRUE;
    }
  }

  if( !is_connecting ){
    check_current_leader(plugin);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void send_peer_list_to_node(node *node, binn *list){
  /* create and send the packet */
  binn *map = binn_map();
  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_PEERS);
  binn_map_set_list (map, PLUGIN_PEERS, list);
  send_peer_message(node, map, NULL);
  binn_free(map);
}

/****************************************************************************/

SQLITE_PRIVATE void send_peer_list(plugin *plugin, node *to_node){
  binn *list = binn_list();
  binn *list2 = binn_list();
  binn *item;
  node *node;

  SYNCTRACE("send_peer_list\n");

  if( !list || !list2 ) return;

  item = binn_list();
  binn_list_add_str(item, to_node->host);
  binn_list_add_int32(item, to_node->bind_port);
  binn_list_add_list(list2, item);
  binn_free(item);

  for(node = plugin->peers; node; node = node->next){
    if( node->is_authorized && node!=to_node && node->bind_port>0 ){
      if( node->id > to_node->id ){
        /* add this node to the list */
        item = binn_list();
        binn_list_add_str(item, node->host);
        binn_list_add_int32(item, node->bind_port);
        binn_list_add_list(list, item);
        binn_free(item);
      }else{  // if( node->id < to_node->id ){
        /* inform the other peer to connect to the new one */
        send_peer_list_to_node(node, list2);
      }
    }
  }

  if( binn_count(list)>0 ){
    send_peer_list_to_node(to_node, list);
  }

  binn_free(list);
  binn_free(list2);
}

/****************************************************************************/

SQLITE_PRIVATE void on_peer_list_request(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_peer_list_request\n");

  send_peer_list(plugin, node);

}

/****************************************************************************/

SQLITE_PRIVATE void request_peer_list(plugin *plugin, node *node){
  binn *map;

  SYNCTRACE("request_peer_list\n");

  map = binn_map();
  binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_GET_PEERS);
  send_peer_message(node, map, NULL);
  binn_free(map);

}

/****************************************************************************/

SQLITE_PRIVATE void start_node_discovery(plugin *plugin) {
  struct tcp_address *address;

  /* send broadcast message to find the peers.
  ** on a separate loop because broadcast addresses can
  ** differ from the bind addresses. eg:
  **  "discovery=192.168.1.255:1234,10.0.34.255:1234"
  **  the same bind address (0.0.0.0:1234) but 2 broadcast addresses.
  */

  if( plugin->broadcast ){
    SYNCTRACE("start_node_discovery - broadcasting node discovery packets\n");
    send_local_udp_broadcast(plugin, "whr?");
  }

  for(address = plugin->discovery; address; address = address->next){
    if( !address->is_broadcast ){
      SYNCTRACE("start_node_discovery - connecting to known node at %s:%d\n", address->host, address->port);
      check_peer_connection(plugin, address->host, address->port);
    }
  }

  /* after each connection: the nodes must share their peers list */

}

/****************************************************************************/

SQLITE_PRIVATE void node_discovery_init(){

  /* a broadcast message to find peers */
  register_udp_message("whr?", on_find_node_request);
  /* a response message informing a peer location */
  register_udp_message("here", on_find_node_response);

}
