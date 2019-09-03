/*

discovery=local:4329

discovery=peer1:port1

discovery=local:4329,peer1:port1

discovery=local:4329,server:port:type

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

  SYNCTRACE("on_udp_message: send a response message to %s\n", sender);

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

  list = binn_map_list(msg, PLUGIN_PEERS);

  SYNCTRACE("peer list received - count=%d\n", binn_count(list) / 2);

  binn_list_foreach(list, item){
    char *host = binn_list_str(&item, 1);
    int   port = binn_list_int32(&item, 2);
    SYNCTRACE("\t node at %s:%d\n", host, port);
    check_peer_connection(plugin, host, port);
  }

}

/****************************************************************************/

SQLITE_PRIVATE void send_peer_list(plugin *plugin, node *to_node){
  binn *list = binn_list();
  node *node;

  SYNCTRACE("send_peer_list\n");

  if( !list ) return;

  for(node = plugin->peers; node; node = node->next){
    if( node!=to_node ){
      binn *item = binn_list();
      binn_list_add_str(item, node->host);
      binn_list_add_int32(item, node->port);
      binn_list_add_list(list, item);
      binn_free(item);
    }
  }

  if( binn_count(list)>0 ){
    /* create and send the packet */
    binn *map = binn_map();
    binn_map_set_int32(map, PLUGIN_CMD, PLUGIN_PEERS);
    binn_map_set_list (map, PLUGIN_PEERS, list);
    send_peer_message(to_node, map, NULL);
    binn_free(map);
  }

  binn_free(list);
}

/****************************************************************************/

#if 0

SQLITE_PRIVATE void on_peer_list_request(node *node, void *msg, int size) {
  plugin *plugin = node->plugin;

  SYNCTRACE("on_peer_list_request\n");

  send_peer_list(plugin, node);

}

/****************************************************************************/

SQLITE_PRIVATE void request_connected_nodes(plugin *plugin, node *node){

  //TODO if required

}

#endif

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
    //send_broadcast_message(plugin, "whr?");
    send_udp_broadcast(plugin, "whr?");
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
