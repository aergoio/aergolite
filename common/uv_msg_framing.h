#ifndef UV_MSG_FRAMING_H
#define UV_MSG_FRAMING_H
#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>


typedef struct uv_msg_s        uv_msg_t;
typedef struct uv_msg_send_s   uv_msg_send_t;


/* Stream Initialization */

int uv_msg_init(uv_loop_t* loop, uv_msg_t* handle, int stream_type);


/* Callback Functions */

typedef void (*uv_free_cb)(uv_handle_t* handle, void* ptr);

typedef void (*uv_msg_read_cb)(uv_msg_t* stream, void *msg, int size);


/* Functions */

int uv_msg_read_start(uv_msg_t* stream, uv_alloc_cb alloc_cb, uv_msg_read_cb msg_read_cb, uv_free_cb free_cb);

int uv_msg_send(uv_msg_send_t* req, uv_msg_t* stream, void* msg, int size, uv_write_cb write_cb);


/* Message Read Structure */

struct uv_msg_s {
   union {
      uv_tcp_t tcp;
      uv_pipe_t pipe;
      void *data;
   };
   char *buf;
   int alloc_size;
   int filled;
   uv_alloc_cb alloc_cb;
   uv_free_cb free_cb;
   uv_msg_read_cb msg_read_cb;
};


/* Message Write Structure */

struct uv_msg_send_s {
   union {
      uv_write_t req;
      void *data;
   };
   uv_buf_t buf[2];
   int msg_size;     /* in network order! */
};


#ifdef __cplusplus
}
#endif
#endif  // UV_MSG_FRAMING_H
