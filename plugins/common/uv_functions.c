/***************************************************************************/
/*** LIBUV MESSAGES ********************************************************/
/***************************************************************************/

SQLITE_PRIVATE void uv_close2(uv_handle_t* handle, uv_close_cb close_cb) {
  if( handle && !uv_is_closing(handle) ){
    uv_close(handle, close_cb);
  }
}

SQLITE_PRIVATE void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*) sqlite3_malloc(suggested_size);
  buf->len = suggested_size;
}

SQLITE_PRIVATE void free_buffer(uv_handle_t* handle, void* ptr) {
  sqlite3_free(ptr);
}

#if TARGET_OS_IPHONE
/* functions bellow not used */
#else

SQLITE_PRIVATE void send_request_on_walk(uv_handle_t *handle, void *arg) {
  SYNCTRACE("on_walk\n");
  uv_close2(handle, NULL);
}

SQLITE_PRIVATE void send_request_on_msg_sent(send_message_t *req, int status) {
   uv_write_t *wreq = (uv_write_t *) req;
   uv_msg_t *stream = (uv_msg_t*) wreq->handle;

   if ( status < 0 ) {
      sqlite3_log(status, "failed to send message to the worker thread: %s", uv_strerror(status));
      uv_close2((uv_handle_t*) stream, NULL);
   } else {
      SYNCTRACE("message sent: %p   user_data: %p\n", (char*)req->msg, req->data);
      if (!stream->data) {
         /* if there is no data to receive (it is just a notification) then close the stream/socket handle */
         uv_close2((uv_handle_t*) stream, NULL);
      }
   }

}

SQLITE_PRIVATE void send_request_on_result(uv_msg_t *stream, void *msg, int size) {

  if (size < 0) {
    if (size != UV_EOF) {
      sqlite3_log(size, "failed to receive result from worker thread: %s", uv_strerror(size));
    }
  }

  if (size > 0) {
    uv_buf_t *response = (uv_buf_t *)stream->data;
    response->base = sqlite3_memdup(msg, size);
  }

  SYNCTRACE("closing socket handle\n");

  uv_close2((uv_handle_t*) stream, NULL);

}

SQLITE_PRIVATE void send_request_on_connect(uv_connect_t* connect, int status){
  uv_msg_t *stream = (uv_msg_t*) connect->handle;
  uv_buf_t *request;
  int rc;

  if (status < 0) {
    sqlite3_log(status, "connection failed on main thread: %s", uv_strerror(status));
    goto loc_failed;
  }

  SYNCTRACE("connected! sending msg...  connect->handle=%p\n", connect->handle);

  /* if a response buffer struct was specified then it must wait for a response */
  if (stream->data) {
    rc = uv_msg_read_start(stream, alloc_buffer, send_request_on_result, free_buffer);
    if (rc < 0) { sqlite3_log(SQLITE_ERROR, "uv_msg_read_start failed on main thread: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }
  }

  /* send the message */
  request = connect->data;
  rc = send_message(stream, request->base, request->len, UV_MSG_STATIC, send_request_on_msg_sent, 0);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "send_message failed on main thread: (%d) %s", rc, uv_strerror(rc)); goto loc_failed; }

  return;
loc_failed:
  uv_close2((uv_handle_t*) stream, NULL);
}

#endif

SQLITE_PRIVATE int send_request_to_worker(plugin *plugin, void *data, int size, uv_buf_t *response){
#if TARGET_OS_IPHONE
  /*
  void *ptr = sqlite3_memdup(data, size);
  if( ptr ){
    SYNCTRACE("send_request_to_worker\n");
    uv_callback_fire(plugin->worker_cb, ptr, NULL);
  }else{
    sqlite3_log(SQLITE_NOMEM, "send_request_to_worker: %s", uv_strerror(SQLITE_NOMEM));
  }
  */
  if( plugin->thread_running ){
    int cmd = *(int*)data;
    SYNCTRACE("send_request_to_worker: 0x%x\n", cmd);
    uv_callback_fire(&plugin->worker_cb, (void*)cmd, NULL);
  }else{
    SYNCTRACE("send_request_to_worker -- the thread is not running!\n");
  }
  (void)response; /* to avoid warning */
  return 0;
#else
  char *address = plugin->worker_address;
  uv_loop_t *loop = uv_default_loop();
  uv_msg_t socket;
  uv_connect_t connect;
  uv_buf_t request;
  int rc;

  rc = uv_msg_init(loop, &socket, UV_NAMED_PIPE);
  if (rc < 0) { sqlite3_log(SQLITE_ERROR, "cannot initialize socket on main thread: (%d) %s", rc, uv_strerror(rc)); return rc; }

  /* save the request data in the connection request */
  request.base = data;
  request.len = size;
  connect.data = &request;

  /* save the pointer to the response buffer struct in the socket */
  socket.data = response;

  SYNCTRACE("send_request - connecting...\n");
  uv_pipe_connect(&connect, (uv_pipe_t*)&socket, address, send_request_on_connect);

  SYNCTRACE("send_request - running event loop\n");
  uv_run(loop, UV_RUN_DEFAULT);

  SYNCTRACE("send_request - exiting event loop, closing handles and loop\n");
  //if (response) {
  //  UVTRACE(("response: %s\n", (char *)response->base));
  //}

  uv_walk(loop, send_request_on_walk, NULL);
  while( uv_run(loop, UV_RUN_DEFAULT)==UV_EBUSY ) {};
  while( uv_loop_close(loop)==UV_EBUSY ) {};

  SYNCTRACE("send_request - returning\n");
  return rc;
#endif
}

/****************************************************************************/

SQLITE_PRIVATE int send_notification_to_worker(plugin *plugin, void *data, int size){
  return send_request_to_worker(plugin, data, size, NULL);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void set_conn_lost(node *node) {

  if (node == NULL) return;

  SYNCTRACE("--- connection lost ---\n");

}

/****************************************************************************/

SQLITE_PRIVATE void disconnect_peer(struct node *node) {
  if( node ){
    SYNCTRACE("disconnect_peer node_id=%d\n", node->id);
    /* close the connection to the peer */
    uv_close2( (uv_handle_t*) &node->socket, worker_thread_on_close);
  }
}

/****************************************************************************/

SQLITE_PRIVATE void on_msg_sent(send_message_t *req, int status) {

 if ( status < 0 ) {
  sqlite3_log(status, "send message failed");
  SYNCTRACE("message send failed: %s   user_data: %p\n", (char*)req->msg, req->data);
 } else {
  SYNCTRACE("message sent: %s   user_data: %p\n", (char*)req->msg, req->data);
 }

}

/****************************************************************************/

// how to send msg to many and only dicard the data when sent to all?
// how to send encrypted msg to many? it would be good to use the encryption only once and send the resulting data...

// 1. wait for callbacks to fire for all of them
//    - it uses less memory
//    - use a counter?
// 2. use UV_MSG_TRANSIENT
//    - easier
//    - will it create a copy of the message even when sending to just 1 destination?
//      maybe when having 2 destination it can use "dynamic" type (pass the free_fn)

SQLITE_PRIVATE BOOL send_peer_message(node *node, binn *map, send_message_cb callback) {
  uv_free_fn free_fn;
  BOOL result=FALSE;
  char *data, *data2;
  int  size;
  int  rc;

  if( !node || !map ) goto loc_exit;
  if( uv_is_closing((uv_handle_t*)&node->socket) ) goto loc_exit;

  switch( node->conn_state ){
  case STATE_CONN_NONE:
  case STATE_CONN_LOST:
  case STATE_ERROR:
    goto loc_exit;
  }

#ifdef DEBUGSYNC
  SYNCTRACE("send_peer_message size=%d ", binn_size(map));

  switch( binn_map_int32(map,PLUGIN_CMD) ){
  case PLUGIN_CMD_ID:             SYNCTRACE("PLUGIN_CMD_ID\n");             break;

  case PLUGIN_REQUEST_STATE_DIFF: SYNCTRACE("PLUGIN_REQUEST_STATE_DIFF\n"); break;
  case PLUGIN_DB_PAGE:            SYNCTRACE("PLUGIN_DB_PAGE\n");            break;
  case PLUGIN_APPLY_UPDATE:       SYNCTRACE("PLUGIN_APPLY_UPDATE\n");       break;
  case PLUGIN_UPTODATE:           SYNCTRACE("PLUGIN_UPTODATE\n");           break;

  case PLUGIN_INSERT_TRANSACTION: SYNCTRACE("PLUGIN_INSERT_TRANSACTION\n"); break;
  case PLUGIN_NEW_TRANSACTION:    SYNCTRACE("PLUGIN_NEW_TRANSACTION\n");    break;
  case PLUGIN_TRANSACTION_FAILED: SYNCTRACE("PLUGIN_TRANSACTION_FAILED\n"); break;

  case PLUGIN_GET_TRANSACTION:    SYNCTRACE("PLUGIN_GET_TRANSACTION\n");    break;
  case PLUGIN_REQUESTED_TRANSACTION: SYNCTRACE("PLUGIN_REQUESTED_TRANSACTION\n"); break;
  case PLUGIN_TXN_NOTFOUND:       SYNCTRACE("PLUGIN_TXN_NOTFOUND\n");       break;

  case PLUGIN_NEW_BLOCK:          SYNCTRACE("PLUGIN_NEW_BLOCK\n");          break;
  case PLUGIN_NEW_BLOCK_ACK:      SYNCTRACE("PLUGIN_NEW_BLOCK_ACK\n");      break;
  case PLUGIN_COMMIT_BLOCK:       SYNCTRACE("PLUGIN_COMMIT_BLOCK\n");       break;

  case PLUGIN_GET_BLOCK:          SYNCTRACE("PLUGIN_GET_BLOCK\n");          break;
  case PLUGIN_REQUESTED_BLOCK:    SYNCTRACE("PLUGIN_REQUESTED_BLOCK\n");    break;
  case PLUGIN_BLOCK_NOTFOUND:     SYNCTRACE("PLUGIN_BLOCK_NOTFOUND\n");     break;

  default:                        SYNCTRACE("UPDATE HERE!\n");              break;
  }
#endif

  data = binn_ptr(map);
  size = binn_size(map);
  //data = binn_release(map);

  data2 = (char*) aergolite_encrypt(node->this_node, (uchar*)data, &size, 0x4329017E);

#if 0
  /* if we sent the encrypted message ... */
  if( data2!=data ){
    /* then we can discard the original message */
    sqlite3_free(data);
    /* and we must set the new destructor */
    free_fn = sqlite3_free;
  }
#endif

  /* if we sent the encrypted message ... */
  if( data2!=data ){
    /* release the new buffer after sending */
    free_fn = sqlite3_free;
  } else {
    /* create a copy of the message */
    free_fn = UV_MSG_TRANSIENT;
  }

  rc = send_message(&node->socket, data2, size, free_fn, callback, node);

  if (rc < 0) {
    SYNCTRACE("--- send_message failed ---\n");
    switch( node->conn_state ){
    case STATE_CONN_NONE:
    case STATE_CONN_LOST:
    case STATE_ERROR:
      break;
    case STATE_UPDATING:
      //if (node->type == NODE_SECONDARY) break;
    default:
      sqlite3_log(rc, "send_peer_message: send_message failed: %s", uv_strerror(rc) );
    }
    set_conn_lost(node);
  } else {
    result = TRUE;
  }

loc_exit:
  return result;

}

/****************************************************************************/

SQLITE_PRIVATE BOOL send_msg(node *node, int cmd, unsigned int arg) {
  BOOL result=FALSE;
  binn *map;

  if ((map = binn_map()) == NULL) goto loc_binn_failed;
  if (binn_map_set_uint32(map, cmd, arg) == FALSE) goto loc_binn_failed;

  result = send_peer_message(node, map, NULL);

loc_exit:
  if (map) binn_free(map);
  return result;

loc_binn_failed:
  sqlite3_log(SQLITE_ERROR, "binn failed: probably out of memory");
  //node->state = STATE_ERROR;  this is not a problem on the node... let the caller function to decide what to do
  goto loc_exit;

}

/****************************************************************************/

SQLITE_PRIVATE BOOL send_text_message(node *node, char *message) {
  BOOL result=FALSE;
  binn *map;

  if ((map = binn_map()) == NULL) goto loc_binn_failed;
  if (binn_map_set_uint32(map, PLUGIN_CMD, PLUGIN_TEXT) == FALSE) goto loc_binn_failed;
  if (binn_map_set_str(map, PLUGIN_TEXT, message) == FALSE) goto loc_binn_failed;

  result = send_peer_message(node, map, NULL);

loc_exit:
  if (map) binn_free(map);
  return result;

loc_binn_failed:
  sqlite3_log(SQLITE_ERROR, "binn failed: probably out of memory");
  goto loc_exit;

}

/****************************************************************************/

#ifdef NOT_BEING_USED
SQLITE_PRIVATE BOOL send_response(node *node, int to_cmd, int cmd, unsigned int arg) {
  BOOL result=FALSE;
  binn *map;

  SYNCTRACE("sending response %x to request %x\n", cmd, to_cmd);

  if ((map = binn_map()) == NULL) goto loc_binn_failed;
  if (binn_map_set_int32(map, REPLICA_RESPONSE, to_cmd) == FALSE) goto loc_binn_failed;
  if (binn_map_set_uint32(map, cmd, arg) == FALSE) goto loc_binn_failed;

  result = send_peer_message(node, map, NULL);

loc_exit:
  if (map) binn_free(map);
  return result;

loc_binn_failed:
  sqlite3_log(SQLITE_ERROR, "binn failed: probably out of memory");
  //node->state = STATE_ERROR;
  goto loc_exit;

}
#endif

/****************************************************************************/

SQLITE_PRIVATE BOOL send_request(node *node, int cmd, unsigned int arg) {

  return send_msg(node, cmd, arg);

}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE int is_local_ip_address(char *address){
  int count=0, i, ret=0;
  uv_interface_address_t *net_interface=NULL;  /* we cannot use the variable name 'interface' on MinGW */

  uv_interface_addresses(&net_interface, &count);
  for(i=0; i<count; i++){
    char local[17] = { 0 };
    int rc = uv_ip4_name(&net_interface[i].address.address4, local, 16);
    if( rc ){
      rc = uv_ip6_name(&net_interface[i].address.address6, local, 16);
    }
    //SYNCTRACE("Local net_interface %d: %s\n", i, local);
    if( strcmp(address,local)==0 ){
      ret = 1;
    }
  }
  uv_free_interface_addresses(net_interface, count);
  return ret;

}

/****************************************************************************/

// use the integer IP address from the sockaddr_in structure
#define INTEGER_IP(SA) SA.sin_addr.s_addr

// option 1: send to the one that starts with 192 -- will not work on some networks
// option 2: send to 255.255.255.255 - it sometimes fails on Android
// option 3: send to all the interfaces, always
// option 4: send to all the interfaces the first time, the next ones use the address that returned responses.

SQLITE_PRIVATE int get_local_broadcast_address(char *list, int size){
  int count=0, i;
  uv_interface_address_t *net_interface=NULL;  /* we cannot use the variable name 'interface' on MinGW */

  //strcpy(list, "255.255.255.255");
  //return SQLITE_OK;

  list[0] = 0;  /* empty the list */

  uv_interface_addresses(&net_interface, &count);
  for(i=0; i<count; i++){
    char broadcast[32];
    broadcast[0] = 0;
    if( net_interface[i].is_internal ) continue;
    if( net_interface[i].address.address4.sin_family==AF_INET ){
      struct sockaddr_in bcast = {0};
      INTEGER_IP(bcast) = INTEGER_IP(net_interface[i].address.address4) |
                         ~INTEGER_IP(net_interface[i].netmask.netmask4);
      uv_ip4_name(&bcast, broadcast, 16);
    //}else if( interface.address.address4.sin_family==AF_INET6 ){
    // TODO: IPv6 uses multicast instead of broadcast
    }
    if( broadcast[0]!=0 ){
      SYNCTRACE("network interface %d - broadcast: %s\n", i, broadcast);
      if( list[0]!=0 ){
        if( strlen(list)+strlen(broadcast)+2 > size ) break;
        strcat(list, ",");
      }
      strcat(list, broadcast);
    }
  }
  uv_free_interface_addresses(net_interface, count);

  return SQLITE_OK;
}

/****************************************************************************/

SQLITE_PRIVATE int get_sockaddr_port(const struct sockaddr *sa) {
  if( sa->sa_family==AF_INET ){
    return ntohs(((struct sockaddr_in*)sa)->sin_port);
  }
  return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

/****************************************************************************/

SQLITE_PRIVATE void get_ip_address(const struct sockaddr *sa, char *name, int size) {
  if( sa->sa_family==AF_INET ){
    uv_ip4_name((struct sockaddr_in*)sa, name, size);
  }else{
    uv_ip6_name((struct sockaddr_in6*)sa, name, size);
  }
}

/****************************************************************************/
/****************************************************************************/

SQLITE_PRIVATE void get_this_device_info(char *hostname, char *cpu_info, char *os_info, char *app_info){
  uv_cpu_info_t *cpus;
  uv_utsname_t os;
  size_t size;
  int count=0;

  /* get CPU info */
  uv_cpu_info(&cpus, &count);
  if( count>0 ){
    strcpy(cpu_info, cpus[0].model);
  }else{
    cpu_info[0] = 0;
  }
  uv_free_cpu_info(cpus, count);

  /* get OS info */
  uv_os_uname(&os);
  snprintf(os_info, 256, "%s %s %s", os.sysname, os.release, os.machine);  // os.version

  /* get hostname */
  size = 256;
  uv_os_gethostname(hostname, &size);

  /* get executable path and name */
  size = 256;
  uv_exepath(app_info, &size);

}
