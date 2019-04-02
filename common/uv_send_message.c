/* This module does not use the uv_msg_send_ prefix name because it should be
   reserved for libuv, which does not handle memory management for requests.
   This module allocates and releases memory used for the requests. */

typedef void (*uv_free_fn) (void *ptr);
#define UV_MSG_STATIC     ((uv_free_fn)0)
#define UV_MSG_TRANSIENT  ((uv_free_fn)-1)

typedef struct send_message_s send_message_t;

typedef void (*send_message_cb) (send_message_t *req, int status);

struct send_message_s {
   union {
      uv_msg_send_t req;
      void *data;
   };
   void *msg;
   uv_free_fn free_fn;
   send_message_cb msg_send_cb;
};

/****************************************************************************/

static void * memdup(void *source, int size) {
  void *ptr;
  if (source == NULL || size <= 0) return NULL;
  ptr = malloc(size);
  if (ptr == 0) return NULL;
  memcpy(ptr, source, size);
  return ptr;
}

static void send_message_completed(uv_write_t *wreq, int status) {
   send_message_t *req = (send_message_t *) wreq;

   /* call the callback function */
   if (req->msg_send_cb) req->msg_send_cb(req, status);

   /* release the message data */
   if (req->free_fn) req->free_fn(req->msg);

   /* release the write request */
   free(req);
}

int send_message(uv_msg_t *socket, char *msg, int size, uv_free_fn free_fn, send_message_cb send_cb, void *user_data) {
   send_message_t *req = malloc(sizeof(send_message_t));

   if (!req) return UV_ENOMEM;

   /* check if we need a copy of the message and save the free function pointer */
   if (free_fn == UV_MSG_TRANSIENT) {
      msg = memdup(msg, size);
      if (!msg) { free(req); return UV_ENOMEM; };
      req->free_fn = free;
   } else {
      req->free_fn = free_fn;
   }

   /* save the message pointer to release on completion */
   req->msg = msg;

   /* save the user data */
   req->data = user_data;

   /* the user callback is optional */
   req->msg_send_cb = send_cb;

   /* send the message */
   return uv_msg_send((uv_msg_send_t*)req, socket, msg, size, send_message_completed);
}
