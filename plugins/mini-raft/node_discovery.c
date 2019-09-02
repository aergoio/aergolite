/*

discovery=local:4329

discovery=peer1:port1

discovery=local:4329,peer1:port1

discovery=local:4329,server:port:type

*/

/****************************************************************************/

/* "whr?": a broadcast message to find peers */
void on_find_node_request(
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
void on_find_node_response(
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

void start_node_discovery(plugin *plugin) {
  struct tcp_address *address;

  /* send broadcast message to find the peers.
  ** on a separate loop because broadcast addresses can
  ** differ from the bind addresses. eg:
  **  "discovery=192.168.1.255:1234,10.0.34.255:1234"
  **  the same bind address (0.0.0.0:1234) but 2 broadcast addresses.
  */
  for (address = plugin->discovery; address; address = address->next) {

    SYNCTRACE("peer connections - sending discovery packets to address: %s:%d\n", address->host, address->port);

    send_broadcast_message(plugin, "whr?");

  }

}

/****************************************************************************/

void node_discovery_init(){

  /* a broadcast message to find peers */
  register_udp_message("whr?", on_find_node_request);
  /* a response message informing a peer location */
  register_udp_message("here", on_find_node_response);

}
