/****************************************************************************/

/* "whr?": a broadcast message to find peers */
void on_find_node_request(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *addr,
  char *sender,
  char *arg
){
  uv_udp_send_t *send_req;
  uv_buf_t response = { .base="here", .len=5 };

  if( is_local_ip_address(sender) ) return;

  /* send a response message */

  SYNCTRACE("on_udp_message: send a response message to %s\n", sender);

  send_req = sqlite3_malloc(sizeof(uv_udp_send_t));
  if( !send_req ) return;

  uv_udp_send(send_req, socket, &response, 1, addr, on_udp_send);

}

/****************************************************************************/

/* "here": a response message informing a peer location */
void on_find_node_response(
  plugin *plugin,
  uv_udp_t *socket,
  const struct sockaddr *addr,
  char *sender,
  char *arg
){

  check_peer_connection(plugin, sender, get_sockaddr_port(addr));

  if( plugin->reconnect_timer_enabled ){
    /* disable the reconnection timer */
    uv_timer_stop(&plugin->reconnect_timer);
    plugin->reconnect_timer_enabled = 0;
    /* enable the after connections timer */
    uv_timer_start(&plugin->after_connections_timer, after_connections_timer_cb, 500, 0);
  }

}

/****************************************************************************/

void node_discovery_init(){

  /* a broadcast message to find peers */
  register_udp_message("whr?", on_find_node_request);
  /* a response message informing a peer location */
  register_udp_message("here", on_find_node_response);

}
