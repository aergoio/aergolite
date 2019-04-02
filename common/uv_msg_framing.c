#include "uv_msg_framing.h"

#ifdef DEBUGTRACE
#define UVTRACE(X)   printf X;
#else
#define UVTRACE(X)
#endif


/* Stream Initialization *****************************************************/

int uv_msg_init(uv_loop_t* loop, uv_msg_t* handle, int stream_type) {
   int rc;

   switch( stream_type ){
   case UV_TCP:
      rc = uv_tcp_init(loop, (uv_tcp_t*) handle);
      break;
   case UV_NAMED_PIPE:
      rc = uv_pipe_init(loop, (uv_pipe_t*) handle, 0);
      break;
   default:
      return UV_EINVAL;
   }

   if( rc ) return rc;

   handle->buf = NULL;
   handle->alloc_size = 0;
   handle->filled = 0;
   handle->alloc_cb = NULL;
   handle->free_cb = NULL;
   handle->msg_read_cb = NULL;
   /* initialize the public member */
   handle->data = NULL;

   return 0;
}


/* Message Writting **********************************************************/

#ifdef _WIN32
static void uv_msg_sent(uv_write_t *req, int status) {
   free(req);
}
#endif

int uv_msg_send(uv_msg_send_t *req, uv_msg_t *socket, void *msg, int size, uv_write_cb write_cb) {
   uv_stream_t *stream = (uv_stream_t*) socket;

   if ( !req || !stream || !msg || size <= 0 ) return UV_EINVAL;

   UVTRACE(("sending message: %s\n", (char*)msg));

   req->msg_size = htonl(size);
   req->buf[0].base = (char*) &req->msg_size;
   req->buf[0].len = 4;
   req->buf[1] = uv_buf_init(msg, size);

#ifdef _WIN32
   /* uv_write does not accept more than 1 buffer with Pipes on Windows
      https://github.com/libuv/libuv/issues/794 */
   if (stream->type == UV_NAMED_PIPE) {
     int rc;
     uv_msg_send_t *req1 = malloc(sizeof(uv_msg_send_t));
     if (!req1) return UV_ENOMEM;
     rc = uv_write((uv_write_t*) req1, stream, &req->buf[0], 1, uv_msg_sent);
     if (rc) { free(req1); return rc; }
     return uv_write((uv_write_t*) req, stream, &req->buf[1], 1, write_cb);
   } else
#endif
   return uv_write((uv_write_t*) req, stream, &req->buf[0], 2, write_cb);

}


/* Message Reading ***********************************************************/

void uv_stream_msg_free_buffer(uv_msg_t *uvmsg) {
   if( uvmsg->free_cb ) uvmsg->free_cb((uv_handle_t*)uvmsg, uvmsg->buf);
   uvmsg->buf = 0;
   uvmsg->alloc_size = 0;
}

int uv_stream_msg_realloc(uv_handle_t *handle, size_t suggested_size) {
   uv_msg_t *uvmsg = (uv_msg_t*) handle;
   uv_buf_t buf = {0};
   uvmsg->alloc_cb(handle, suggested_size, &buf);
   if( buf.base==0 || buf.len < suggested_size ) return 0;  //! if buf.len < suggested_size and buf.base is valid it will be lost here (the allocated memory)
   memcpy(buf.base, uvmsg->buf, uvmsg->filled);
   if( uvmsg->free_cb ) uvmsg->free_cb(handle, uvmsg->buf);
   uvmsg->buf = buf.base;
   uvmsg->alloc_size = buf.len;
   return 1;
}

void uv_stream_msg_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *stream_buf) {
   uv_msg_t *uvmsg = (uv_msg_t*) handle;

   UVTRACE(("stream_msg_alloc  uvmsg=%p\n", uvmsg));
   if( uvmsg==0 ) return;

   if( uvmsg->buf==0 ){
      uv_buf_t buf = {0};
      uvmsg->alloc_cb(handle, suggested_size, &buf);
      uvmsg->buf = buf.base;
      if( uvmsg->buf==0 ) return;
      uvmsg->alloc_size = buf.len;
   }

   UVTRACE(("stream_msg_alloc  uvmsg->buf=%p  filled=%d\n", uvmsg->buf, uvmsg->filled));

   if( uvmsg->filled >= 4 ){
      int msg_size = ntohl(*(int*)uvmsg->buf);
      int entire_msg_size = msg_size + 4;
      UVTRACE(("stream_msg_alloc  msg_size=%d\n", msg_size));
      if( uvmsg->alloc_size < entire_msg_size ){
         /* here the suggested size is exactly what it's needed to read the entire message */
         if( !uv_stream_msg_realloc(handle, entire_msg_size) ){
            stream_buf->base = 0;
            return;
         }
      }
      stream_buf->len = entire_msg_size - uvmsg->filled;
   } else {
      if( uvmsg->alloc_size < 4 ){
         /* There is no enough space for the message length. Allocate the default size */
         UVTRACE(("calling realloc - alloc_size: %d, filled: %d\n", uvmsg->alloc_size, uvmsg->filled));
         if( !uv_stream_msg_realloc(handle, 64 * 1024) ){
            stream_buf->base = 0;
            return;
         }
      }
      stream_buf->len = uvmsg->alloc_size - uvmsg->filled;
   }

   stream_buf->base = uvmsg->buf + uvmsg->filled;
   UVTRACE(("stream_msg_alloc  base=%p  len=%d\n", stream_buf->base, stream_buf->len));
}

void uv_stream_msg_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
   uv_msg_t *uvmsg = (uv_msg_t*) stream;
   char *ptr;

   UVTRACE(("uv_stream_msg_read: received %d bytes\n", nread));
   UVTRACE(("uvmsg: %p  uvmsg->buf: %p  buf->base: %p\n", uvmsg, uvmsg->buf, buf->base));

   if (uvmsg == 0) return;

   if (nread == 0) {
      /* Nothing read */
      //! does it should release the ->buf here?
      //uv_stream_msg_free_buffer(uvmsg);
      return;
   }

   if (nread < 0) {
      /* Error */
      uv_stream_msg_free_buffer(uvmsg);
      uvmsg->msg_read_cb((uv_msg_t*)stream, NULL, nread);
      return;
   }

#ifdef TESTING_UV_MSG_FRAMING
   assert(buf->base == uvmsg->buf + uvmsg->filled);
   print_bytes("received", buf->base, nread);
#endif

   uvmsg->filled += nread;

   UVTRACE(("alloc_size: %d, received: %d, filled: %d\n", uvmsg->alloc_size, nread, uvmsg->filled));

   ptr = uvmsg->buf;

   while( uvmsg->filled >= 4 ){
      int msg_size = ntohl(*(int*)ptr);
      int entire_msg = msg_size + 4;
      UVTRACE(("msg_size: %d, entire_msg: %d\n", msg_size, entire_msg));
      if( uvmsg->filled >= entire_msg ){
         uvmsg->msg_read_cb((uv_msg_t*)stream, ptr + 4, msg_size);
         if( uvmsg->filled > entire_msg ){
            ptr += entire_msg;
         }
         uvmsg->filled -= entire_msg;
      } else {
         break;
      }
   }

   if( ptr > uvmsg->buf && uvmsg->filled > 0 ){
      UVTRACE(("moving the buffer\n"));
      memmove(uvmsg->buf, ptr, uvmsg->filled);
   } else if( uvmsg->filled == 0 ){
      UVTRACE(("releasing the buffer\n"));
      uv_stream_msg_free_buffer(uvmsg);
   }

#ifdef TESTING_UV_MSG_FRAMING
   uv_async_send(&async_next_step);
#endif
}

int uv_msg_read_start(uv_msg_t* stream, uv_alloc_cb alloc_cb, uv_msg_read_cb msg_read_cb, uv_free_cb free_cb) {

   stream->msg_read_cb = msg_read_cb;
   stream->alloc_cb = alloc_cb;
   stream->free_cb = free_cb;

   return uv_read_start((uv_stream_t*)stream, uv_stream_msg_alloc, uv_stream_msg_read);

}
