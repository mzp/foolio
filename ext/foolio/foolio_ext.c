#include <ruby.h>
#include <uv.h>
#include <stdbool.h>

void *  rb_thread_call_with_gvl(void *(*func)(void *), void *data1);

#define Wrap(expr, f)   Data_Wrap_Struct(klass, 0, (f), (expr))
#define Method(name, n) rb_define_singleton_method(klass, #name, foolio_##name, n)
#define Decl(T) \
  uv_##T##_t * T##_p; \
  Data_Get_Struct(T, uv_##T##_t, T##_p)

#define InitHandle(T)                                \
  uv_##T##_t * handle = malloc(sizeof(uv_##T##_t));  \
  Decl(loop);                                        \
  if( uv_##T##_init(loop_p, handle) == 0) {          \
    return Data_Wrap_Struct(klass, 0, 0, handle);    \
  } else {                                           \
    return Qnil;                                     \
  }                                                  \

#define CHECK(X) \
  if(X == -1) { \
    rb_raise(rb_eException, "libuv failed by %s", uv_strerror(uv_last_error(handle_->loop))); \
  }

// Foolio::UV class
VALUE klass;

struct LoopData {
  bool gvl; // has GVL?
};

// ------------------------------
// callback
// ------------------------------
struct CallbackData {
  VALUE cb;
  int argc;
  VALUE* argv;

  VALUE on_close;
};

struct CallbackData* callback(VALUE cb) {
  struct CallbackData* data = malloc(sizeof(struct CallbackData));
  data->cb = cb;
  data->argc = 0;
  data->argv = NULL;
  return data;
}

static void* foolio__cb__apply(void* p) {
  struct CallbackData* data = (struct CallbackData*)p;
  VALUE arity =
    rb_funcall2(data->cb, rb_intern("arity"), 0, NULL);
  if(NUM2INT(arity) != data->argc) {
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", data->argc, NUM2INT(arity));
  } else {
    rb_funcall2(data->cb, rb_intern("call"), data->argc, data->argv);
    return NULL;
  }
}

static void foolio__cb_apply(uv_loop_t* loop, struct CallbackData* data, int argc, VALUE argv[]) {
  struct LoopData* p = loop->data;
  data->argc = argc;
  data->argv = argv;
  if(p->gvl) {
    foolio__cb__apply((void*)data);
  } else {
    // All callback function are run without gvl.
    // But we need GVL to call Ruby methods.
    p->gvl = true;
    rb_thread_call_with_gvl(foolio__cb__apply, (void*)data);
    p->gvl = false;
  }
}

void foolio__cb_free(struct CallbackData* data) {
  free(data);
}

// ------------------------------
// Loop
// ------------------------------
VALUE foolio_default_loop(VALUE self) {
  return Wrap( uv_default_loop(), 0 );
}

VALUE foolio_loop_new(VALUE self){
  return Wrap( uv_loop_new(), 0);
}

VALUE foolio_loop_delete(VALUE self, VALUE loop) {
  Decl(loop);
  uv_loop_delete(loop_p);
  return Qnil;
}

/*
typedef void (*uv_walk_cb)(uv_handle_t* handle, void* arg);
typedef void (*uv_close_cb)(uv_handle_t* handle);
UV_EXTERN void uv_walk(uv_loop_t* loop, uv_walk_cb walk_cb, void* arg);
UV_EXTERN void uv_close(uv_handle_t* handle, uv_close_cb close_cb);
*/
void foolio__close_cb(uv_handle_t* handle) {
  struct CallbackData* data = (struct CallbackData*)handle->data;
  data->cb = data->on_close;
  foolio__cb_apply(handle->loop, data, 0, 0);
  foolio__cb_free(handle->data);
  free(handle);
}

void foolio__close(uv_handle_t* handle, VALUE cb) {
  if(uv_is_closing(handle)){ return; }

  if(handle->data == NULL) {
    handle->data = malloc(sizeof(struct CallbackData*));
  }
  ((struct CallbackData*)handle->data)->on_close = (VALUE)cb;

  uv_close(handle, foolio__close_cb);
}

void foolio__close_all(uv_handle_t* handle, void* cb){
  foolio__close(handle, (VALUE)cb);
}

VALUE foolio_close_all(VALUE self, VALUE loop, VALUE cb) {
  uv_loop_t* loop_;
  Data_Get_Struct(loop, uv_loop_t, loop_);
  uv_walk(loop_, foolio__close_all, (void*)cb);
}

VALUE foolio_close(VALUE self, VALUE handle, VALUE cb) {
  uv_handle_t* handle_;
  Data_Get_Struct(handle, uv_handle_t, handle_);
  foolio__close(handle_, cb);
}

VALUE foolio_is_active(VALUE handle) {
  uv_handle_t* handle_;
  Data_Get_Struct(handle, uv_handle_t, handle_);
  int retval = uv_is_active(handle_);
  CHECK(retval);
  return INT2NUM(retval);
}

// ------------------------------
// run
// ------------------------------
VALUE foolio__run(void* loop) {
  return INT2NUM(uv_run(loop));
}

VALUE foolio_run(VALUE self, VALUE loop) {
  Decl(loop);
  struct LoopData data = { false };
  loop_p->data = (void*)&data;
  rb_thread_blocking_region(foolio__run, (void*)loop_p, RUBY_UBF_IO, NULL);
}

VALUE foolio_run_once(VALUE self, VALUE loop) {
  Decl(loop);
  struct LoopData data = { true };
  loop_p->data = (void*)&data;
  return INT2NUM(uv_run_once(loop_p));
}

VALUE foolio_ip4_addr(VALUE self, VALUE ip, VALUE port) {
  struct sockaddr_in* p = malloc(sizeof(struct sockaddr_in));
  *p = uv_ip4_addr((const char*)StringValueCStr(ip), NUM2INT(port));
  return Wrap(p, free);
}

VALUE foolio_ip_name(VALUE self, VALUE src) {
  struct sockaddr_in* addr_;
  Data_Get_Struct(src, struct sockaddr_in, addr_);
  if(addr_ == NULL) { return Qnil; }
  if(addr_->sin_family == AF_INET6) {
    char str[INET6_ADDRSTRLEN];
    uv_ip6_name((struct sockaddr_in6*)addr_, str, INET_ADDRSTRLEN);
    return rb_str_new_cstr(str);
  } else {
    char str[INET_ADDRSTRLEN];
    uv_ip4_name(addr_, str, INET_ADDRSTRLEN);
    return rb_str_new_cstr(str);
  }
}

VALUE foolio_port(VALUE self, VALUE src) {
  struct sockaddr_in* addr_;
  Data_Get_Struct(src, struct sockaddr_in, addr_);
  if(addr_ == NULL) { return Qnil; }
  if(addr_->sin_family == AF_INET6) {
    int port  = ntohs(((struct sockaddr_in6*)addr_)->sin6_port);
    return INT2FIX(port);
  } else {
    int port  = ntohs(addr_->sin_port);
    return INT2FIX(port);
  }
}

/*VALUE foolio_ip6_name(VALUE self, VALUE src) {
  struct sockaddr_in6* addr_;
  Data_Get_Struct(addr, struct sockaddr_in6, addr_);
  char str[INET_ADDRSTRLEN];
  uv_ip4_name(addr_, str, INET_ADDRSTRLEN);
  return rb_str_new_cstr(str);
  }*/


VALUE foolio_ip6_addr(VALUE self, VALUE ip, VALUE port) {
  struct sockaddr_in6* p = malloc(sizeof(struct sockaddr_in6));
  *p = uv_ip6_addr((const char*)StringValueCStr(ip), NUM2INT(port));
  return Wrap(p, free);
}

void foolio__connection_cb(uv_stream_t* server, int status) {
  VALUE argv[] = { INT2FIX(status) };
  foolio__cb_apply(server->loop, server->data, 1, argv);
}

// UV_EXTERN int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb)
VALUE foolio_listen(VALUE self, VALUE stream, VALUE backlog, VALUE cb) {
  uv_stream_t* handle_;
  Data_Get_Struct(stream, uv_stream_t, handle_);
  int backlog_ = NUM2INT(backlog);
  handle_->data = (void*)callback(cb);
  int retval = uv_listen(handle_, backlog_, foolio__connection_cb);
  CHECK(retval);
  return INT2FIX(retval);
}

// EXTERN int uv_accept(uv_stream_t* server, uv_stream_t* client);
VALUE foolio_accept(VALUE self, VALUE handle, VALUE client) {
  uv_stream_t* handle_;
  Data_Get_Struct(handle, uv_stream_t, handle_);
  uv_stream_t* client_;
  Data_Get_Struct(client, uv_stream_t, client_);
  //uv_stream_t* server, uv_stream_t* client) {
  int retval = uv_accept(handle_, client_);
  CHECK(retval);
  return INT2FIX(retval);
}

uv_buf_t foolio__alloc(uv_handle_t* handle, size_t suggested_size) {
  return uv_buf_init((char*)malloc(suggested_size), (unsigned int)suggested_size);
}

struct MakeBufArg {
  char* ptr;
  ssize_t size;
};

void* foolio___make_buf(void* args) {
  struct MakeBufArg* args_ = (struct MakeBufArg*) args;
  return (void*)rb_str_new(args_->ptr, args_->size);
}

VALUE foolio__make_buf(char* ptr, ssize_t size) {
  struct MakeBufArg args = { ptr, size };
  if(size == -1) {
    return Qnil;
  } else {
    free(ptr);
    return (VALUE)rb_thread_call_with_gvl(foolio___make_buf, &args);
  }
}

void foolio__read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf){
  VALUE argv[] = { foolio__make_buf(buf.base, nread) };
  for(int i = 0; i < nread; ++i){ 
printf("%x", buf.base[i]);
  }
  printf("\n");
  foolio__cb_apply(stream->loop, stream->data, 1, argv);
}

void foolio__read2_cb(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
  VALUE argv[] = { foolio__make_buf(buf.base, nread), INT2FIX(pending) };
  foolio__cb_apply(pipe->loop, pipe->data, 2, argv);
}

// UV_EXTERN int uv_read_start(uv_stream_t*, uv_alloc_cb alloc_cb, uv_read_cb read_cb)
static VALUE foolio_read_start(VALUE self, VALUE stream, VALUE read_cb) {
  uv_stream_t* handle_;
  Data_Get_Struct(stream, uv_stream_t, handle_);
  handle_->data = (void*)callback(read_cb);
  int retval = uv_read_start(handle_, foolio__alloc, foolio__read_cb);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_read_stop(uv_stream_t*)
VALUE foolio_read_stop(VALUE self, VALUE stream) {
  uv_stream_t* handle_;
  Data_Get_Struct(stream, uv_stream_t, handle_);
  int retval = uv_read_stop(handle_);
  foolio__cb_free(handle_->data);
  handle_->data = NULL;
  CHECK(retval);
  return NUM2INT(retval);
}

// UV_EXTERN int uv_read2_start(uv_stream_t*, uv_alloc_cb alloc_cb, uv_read2_cb read_cb)
VALUE foolio_read2_start(VALUE self, VALUE stream, VALUE read_cb) {
  uv_stream_t* handle_;
  Data_Get_Struct(stream, uv_stream_t, handle_);
  handle_->data = (void*)callback(read_cb);
  int retval = uv_read2_start(handle_, foolio__alloc, foolio__read2_cb);
  CHECK(retval);
  return INT2NUM(retval);
}

void foolio__write_cb(uv_write_t* req, int status){
  VALUE argv[] = { INT2FIX(status) };
  foolio__cb_apply(req->handle->loop, req->handle->data, 1, argv);
}

// UV_EXTERN int uv_write(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt, uv_write_cb cb)
VALUE foolio_write(VALUE self, VALUE req, VALUE handle, VALUE buf, VALUE cb) {
  uv_write_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_write_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_write_t, req_);
  }
  uv_stream_t* handle_;
  Data_Get_Struct(handle, uv_stream_t, handle_);
  uv_buf_t bufs[] = {
    uv_buf_init(StringValueCStr(buf), (int)rb_str_strlen(buf))
  };
  handle_->data = (void*)callback(cb);
  int retval = uv_write(req_, handle_, bufs, 1, foolio__write_cb);
  CHECK(retval);
  return retval == 0 ? req : Qnil;
}

// UV_EXTERN int uv_write2(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt, uv_stream_t* send_handle, uv_write_cb cb)
VALUE foolio_write2(VALUE self, VALUE req, VALUE handle, VALUE buf, VALUE send_handle, VALUE cb) {
  uv_write_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_write_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_write_t, req_);
  }
  uv_stream_t* handle_;
  Data_Get_Struct(handle, uv_stream_t, handle_);
  uv_buf_t bufs[] = {
    uv_buf_init(StringValueCStr(buf), (int)rb_str_strlen(buf))
  };
  uv_stream_t* send_handle_;
  Data_Get_Struct(send_handle, uv_stream_t, send_handle_);
  handle_->data = (void*)callback(cb);
  int retval = uv_write2(req_, handle_, bufs, 1, send_handle_, foolio__write_cb);
  return retval == 0 ? req : Qnil;
}

// UV_EXTERN int uv_is_readable(const uv_stream_t* handle)
VALUE foolio_is_readable(VALUE self, VALUE handle) {
  const uv_stream_t* handle_;
  Data_Get_Struct(handle, const uv_stream_t, handle_);
  int retval = uv_is_readable(handle_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_is_writable(const uv_stream_t* handle)
VALUE foolio_is_writable(VALUE self, VALUE handle) {
  const uv_stream_t* handle_;
  Data_Get_Struct(handle, const uv_stream_t, handle_);
  int retval = uv_is_writable(handle_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_is_closing(const uv_handle_t* handle)
VALUE foolio_is_closing(VALUE self, VALUE handle) {
  const uv_handle_t* handle_;
  Data_Get_Struct(handle, const uv_handle_t, handle_);
  int retval = uv_is_closing(handle_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_init(uv_loop_t*, uv_udp_t* handle)
VALUE foolio_udp_init(VALUE self, VALUE loop) {
  InitHandle(udp);
}

// UV_EXTERN int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr, unsigned int flags)
VALUE foolio_udp_bind(VALUE self, VALUE handle, VALUE addr, VALUE flags) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  struct sockaddr_in* addr_;
  Data_Get_Struct(addr, struct sockaddr_in, addr_);
  unsigned int flags_ = NUM2UINT(flags);
  int retval = uv_udp_bind(handle_, *addr_, flags_);
    CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr, unsigned int flags)
VALUE foolio_udp_bind6(VALUE self, VALUE handle, VALUE addr, VALUE flags) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  struct sockaddr_in6* addr_;
  Data_Get_Struct(addr, struct sockaddr_in6, addr_);
  unsigned int flags_ = NUM2UINT(flags);
  int retval = uv_udp_bind6(handle_, *addr_, flags_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_getsockname(uv_udp_t* handle, struct sockaddr* name, int* namelen)
VALUE foolio_udp_getsockname(VALUE self, VALUE handle, VALUE name, VALUE namelen) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  struct sockaddr* name_ = malloc(sizeof(struct sockaddr));
  int size;
  int retval = uv_udp_getsockname(handle_, name_, &size);
  CHECK(retval);
  return Wrap(name_, free);
}

// UV_EXTERN int uv_udp_set_membership(uv_udp_t* handle, const char* multicast_addr, const char* interface_addr, uv_membership membership)
VALUE foolio_udp_set_membership(VALUE self, VALUE handle, VALUE multicast_addr, VALUE interface_addr, VALUE membership) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  const char* multicast_addr_ = StringValueCStr(multicast_addr);
  const char* interface_addr_ = StringValueCStr(interface_addr);
  uv_membership membership_ =  NUM2INT(membership);
  int retval = uv_udp_set_membership(handle_, multicast_addr_, interface_addr_, membership_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_set_multicast_loop(uv_udp_t* handle, int on)
VALUE foolio_udp_set_multicast_loop(VALUE self, VALUE handle, VALUE on) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  int on_ = NUM2INT(on);
  int retval = uv_udp_set_multicast_loop(handle_, on_);
  CHECK(retval);
  return INT2NUM(retval);
}


// UV_EXTERN int uv_udp_set_multicast_ttl(uv_udp_t* handle, int ttl)
VALUE foolio_udp_set_multicast_ttl(VALUE self, VALUE handle, VALUE ttl) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  int ttl_ = NUM2INT(ttl);
  int retval = uv_udp_set_multicast_ttl(handle_, ttl_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_set_broadcast(uv_udp_t* handle, int on)
VALUE foolio_udp_set_broadcast(VALUE self, VALUE handle, VALUE on) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  int on_ = NUM2INT(on);
  int retval = uv_udp_set_broadcast(handle_, on_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_set_ttl(uv_udp_t* handle, int ttl)
VALUE foolio_udp_set_ttl(VALUE self, VALUE handle, VALUE ttl) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  int ttl_ = NUM2INT(ttl);
  int retval = uv_udp_set_ttl(handle_, ttl_);
  CHECK(retval);
  return INT2NUM(retval);
}

void foolio__udp_send_cb(uv_udp_send_t* req, int status) {
  VALUE argv[] = { INT2FIX(status) };
  foolio__cb_apply(req->handle->loop, req->handle->data, 1 , argv);
}

// UV_EXTERN int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[], int bufcnt, struct sockaddr_in addr, uv_udp_send_cb send_cb)
VALUE foolio_udp_send(VALUE self, VALUE req, VALUE handle, VALUE buf, VALUE addr, VALUE send_cb) {
  uv_udp_send_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_udp_send_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_udp_send_t, req_);
  }
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);

  struct sockaddr_in* addr_;
  Data_Get_Struct(addr, struct sockaddr_in, addr_);
  uv_buf_t bufs[] = {
    uv_buf_init(StringValueCStr(buf), (int)rb_str_strlen(buf))
  };
  handle_->data = (void*)callback(send_cb);
  int retval = uv_udp_send(req_, handle_, bufs, 1 , *addr_, foolio__udp_send_cb);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_send6(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[], int bufcnt, struct sockaddr_in6 addr, uv_udp_send_cb send_cb)
VALUE foolio_udp_send6(VALUE self, VALUE req, VALUE handle, VALUE buf, VALUE addr, VALUE send_cb) {
    uv_udp_send_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_udp_send_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_udp_send_t, req_);
  }
  Data_Get_Struct(req, uv_udp_send_t, req_);
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  uv_buf_t bufs[] = {
    uv_buf_init(StringValueCStr(buf), (int)rb_str_strlen(buf))
  };
  struct sockaddr_in6* addr_;
  Data_Get_Struct(addr, struct sockaddr_in6, addr_);
  handle_->data = (void*)callback(send_cb);
  int retval = uv_udp_send6(req_, handle_, bufs, 1, *addr_, foolio__udp_send_cb);
  CHECK(retval);
  return INT2NUM(retval);
}

void* safe__wrap(void* ptr) {
  return (void*)Wrap(ptr, NULL);
}
VALUE safe_wrap(void* ptr) {
  return (VALUE)rb_thread_call_with_gvl(safe__wrap, ptr);
}

void foolio__udp_recv_cb(uv_udp_t* handle, ssize_t nread, uv_buf_t buf, struct sockaddr* addr, unsigned int flags) {
  VALUE argv[] = {
    foolio__make_buf(buf.base, nread),
    safe_wrap((void*)addr),
    INT2FIX(flags) };
  foolio__cb_apply(handle->loop, handle->data, 3, argv);
}

// UV_EXTERN int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb)
VALUE foolio_udp_recv_start(VALUE self, VALUE handle, VALUE recv_cb) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  handle_->data = (void*)callback(recv_cb);
  int retval = uv_udp_recv_start(handle_, foolio__alloc, foolio__udp_recv_cb);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_udp_recv_stop(uv_udp_t* handle)
VALUE foolio_udp_recv_stop(VALUE self, VALUE handle) {
  uv_udp_t* handle_;
  Data_Get_Struct(handle, uv_udp_t, handle_);
  int retval = uv_udp_recv_stop(handle_);
  foolio__cb_free(handle_->data);
  handle_->data = NULL;
  CHECK(retval);
  return INT2NUM(retval);
}

VALUE foolio__str(const char* str) {
  return (VALUE)rb_thread_call_with_gvl((void *(*)(void *))rb_str_new_cstr, (void*)str);
}

void foolio__fs_event_cb(uv_fs_event_t* handle, const char* filename, int events, int status) {
  VALUE argv[] = { INT2FIX(events), INT2FIX(status) };
  foolio__cb_apply(handle->loop, handle->data, 2, argv);
}

VALUE foolio_tcp_init(VALUE self, VALUE loop) {
  InitHandle(tcp);
}

// UV_EXTERN int uv_tcp_nodelay(uv_tcp_t* handle, int enable)
VALUE foolio_tcp_nodelay(VALUE self, VALUE handle, VALUE enable) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  int retval = uv_tcp_nodelay(handle_, enable_);
  CHECK(retval);
  return Qnil;
}

// UV_EXTERN int uv_tcp_keepalive(uv_tcp_t* handle, int enable,unsigned int delay)
VALUE foolio_tcp_keepalive(VALUE self, VALUE handle, VALUE enable, VALUE delay) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  unsigned int delay_ = NUM2UINT(delay);
  int retval = uv_tcp_keepalive(handle_, enable_, delay_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable)
VALUE foolio_tcp_simultaneous_accepts(VALUE self, VALUE handle, VALUE enable) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  int retval = uv_tcp_simultaneous_accepts(handle_, enable_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in)
VALUE foolio_tcp_bind(VALUE self, VALUE handle, VALUE sockaddr_in) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in* sockaddr_in_;
  Data_Get_Struct(sockaddr_in, struct sockaddr_in, sockaddr_in_);
  int retval = uv_tcp_bind(handle_, *sockaddr_in_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6)
VALUE foolio_tcp_bind6(VALUE self, VALUE handle, VALUE sockaddr_in6) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in6* sockaddr_in6_;
  Data_Get_Struct(sockaddr_in6, struct sockaddr_in6, sockaddr_in6_);

  int retval = uv_tcp_bind6(handle_, *sockaddr_in6_);
  CHECK(retval);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,    int* namelen)
VALUE foolio_tcp_getsockname(VALUE self, VALUE handle) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr* name_ = malloc(sizeof(struct sockaddr));
  int size;
  int retval = uv_tcp_getsockname(handle_, name_, &size);
  CHECK(retval);
  return Wrap(name_, free);
}

// UV_EXTERN int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,    int* namelen)
VALUE foolio_tcp_getpeername(VALUE self, VALUE handle, VALUE name, VALUE namelen) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr* name_ = malloc(sizeof(struct sockaddr));
  int size;
  int retval = uv_tcp_getpeername(handle_, name_, &size);
  CHECK(retval);
  return Wrap(name_, free);
}

void foolio__connect_cb(uv_connect_t* req, int status) {
  VALUE argv[] = { INT2FIX(status) };
  foolio__cb_apply(req->handle->loop, req->handle->data, 1, argv);
}

// UV_EXTERN int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle, struct sockaddr_in address, uv_connect_cb cb)
VALUE foolio_tcp_connect(VALUE self, VALUE req, VALUE handle, VALUE address, VALUE cb) {
  uv_connect_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_connect_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_connect_t, req_);
  }
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in* address_;
  Data_Get_Struct(address, struct sockaddr_in, address_);
  handle_->data = (void*)callback(cb);
  int retval = uv_tcp_connect(req_, handle_, *address_, foolio__connect_cb);
  CHECK(retval);
  return req;
}

// UV_EXTERN int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,    struct sockaddr_in6 address, uv_connect_cb cb)
VALUE foolio_tcp_connect6(VALUE self, VALUE req, VALUE handle, VALUE address, VALUE cb) {
  uv_connect_t* req_;
  if( req == Qnil) {
    req_ = malloc(sizeof(uv_connect_t));
    req  = Wrap(req_, free);
  } else {
    Data_Get_Struct(req, uv_connect_t, req_);
  }
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in6* address_;
  Data_Get_Struct(address, struct sockaddr_in6, address_);
  req_->data = (void*)callback(cb);
  int retval = uv_tcp_connect6(req_, handle_, *address_, foolio__connect_cb);
  CHECK(retval);
  return req;
}

/*
  UV_EXTERN int uv_idle_init(uv_loop_t*, uv_idle_t* idle);
  UV_EXTERN int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);
  UV_EXTERN int uv_idle_stop(uv_idle_t* idle);
*/
VALUE foolio_idle_init(VALUE self, VALUE loop) {
  InitHandle(idle);
}

void foolio__idle_cb(uv_idle_t* handle, int status) {
  VALUE argv[] = { INT2FIX(status) };
  foolio__cb_apply(handle->loop, handle->data,1, argv);
}

VALUE foolio_idle_start(VALUE self, VALUE handle, VALUE cb) {
  uv_idle_t* handle_;
  Data_Get_Struct(handle, uv_idle_t, handle_);
  handle_->data = callback(cb);
  int retval = uv_idle_start(handle_, foolio__idle_cb);
  CHECK(retval);
  return INT2FIX(retval);
}

VALUE foolio_idle_stop(VALUE self, VALUE handle) {
  uv_idle_t* handle_;
  Data_Get_Struct(handle, uv_idle_t, handle_);
  foolio__cb_free(handle_->data);
  handle_->data = NULL;
  int retval = uv_idle_stop(handle_);
  CHECK(retval);
  return INT2FIX(retval);
}

/**
   UV_EXTERN int uv_timer_init(uv_loop_t*, uv_timer_t* timer);
   UV_EXTERN int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb,
   UV_EXTERN int uv_timer_stop(uv_timer_t* timer);
   UV_EXTERN int uv_timer_again(uv_timer_t* timer);
   UV_EXTERN void uv_timer_set_repeat(uv_timer_t* timer, int64_t repeat);
   UV_EXTERN int64_t uv_timer_get_repeat(uv_timer_t* timer);
 */
VALUE foolio_timer_init(VALUE self, VALUE loop) {
  InitHandle(timer);
}

static void timer_callback(uv_timer_t* handle, int status) {
  VALUE argv[] = { INT2NUM(status) };
  foolio__cb_apply(handle->loop, handle->data, 1, argv);
}

VALUE foolio_timer_start(VALUE self, VALUE handle, VALUE cb, VALUE timeout, VALUE repeat) {
  uv_timer_t* handle_;
  Data_Get_Struct(handle, uv_timer_t, handle_);
  handle_->data = (void*)callback(cb);
  int retval =
    uv_timer_start(handle_, timer_callback, NUM2INT(timeout), NUM2INT(repeat));
  CHECK(retval);
  return INT2NUM(retval);
}

VALUE foolio_timer_stop(VALUE self, VALUE handle) {
  uv_timer_t* handle_;
  Data_Get_Struct(handle, uv_timer_t, handle_);
  int retval = uv_timer_stop(handle_);
  foolio__cb_free(handle_->data);
  handle_->data = NULL;
  CHECK(retval);
  return INT2FIX(retval);
}

VALUE foolio_timer_again(VALUE self, VALUE handle) {
  uv_timer_t* handle_;
  Data_Get_Struct(handle, uv_timer_t, handle_);
  int retval = uv_timer_again(handle_);
  CHECK(retval);
  return INT2FIX(retval);
}

VALUE foolio_timer_set_repeat(VALUE self, VALUE handle, VALUE repeat) {
  uv_timer_t* handle_;
  Data_Get_Struct(handle, uv_timer_t, handle_);
  int64_t repeat_n = NUM2LONG(repeat);
  uv_timer_set_repeat(handle_, repeat);
  return Qnil;
}

VALUE foolio_timer_get_repeat(VALUE self, VALUE timer) {
  Decl(timer);
  int64_t retval = uv_timer_get_repeat(timer_p);
  return LONG2NUM(retval);
}

// UV_EXTERN int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle, const char* filename, uv_fs_event_cb cb, int flags)
VALUE foolio_fs_event_init(VALUE self, VALUE loop, VALUE filename, VALUE cb, VALUE flags) {
  uv_loop_t* loop_;
  Data_Get_Struct(loop, uv_loop_t, loop_);
  uv_fs_event_t* handle_ = malloc(sizeof(uv_fs_event_t));
  const char* filename_ = StringValueCStr(filename);
  handle_->data = (void*)callback(cb);
  int flags_ = FIX2INT(flags);
  int retval = uv_fs_event_init(loop_, handle_, filename_, foolio__fs_event_cb, flags_);
  CHECK(retval);
  return Wrap(handle_, 0);
}

// UV_EXTERN int uv_pipe_init(uv_loop_t*, uv_pipe_t* handle, int ipc)
VALUE foolio_pipe_init(VALUE self, VALUE loop, VALUE ipc) {
  uv_loop_t* loop_;
  Data_Get_Struct(loop, uv_loop_t, loop_);
  int ipc_ = NUM2INT(ipc);

  uv_pipe_t * handle = malloc(sizeof(uv_pipe_t));
  if( uv_pipe_init(loop_, handle, ipc_) == 0) {
    return Data_Wrap_Struct(klass, 0, 0, handle);
  } else {
    return Qnil;
  }
}

// UV_EXTERN void uv_pipe_open(uv_pipe_t*, uv_file file)
VALUE foolio_pipe_open(VALUE self, VALUE handle, VALUE file) {
  uv_pipe_t* handle_;
  Data_Get_Struct(handle, uv_pipe_t, handle_);
  uv_file file_ = NUM2INT(file);
  uv_pipe_open(handle_, file_);
  return Qnil;
}

// UV_EXTERN int uv_pipe_bind(uv_pipe_t* handle, const char* name)
VALUE foolio_pipe_bind(VALUE self, VALUE handle, VALUE name) {
  uv_pipe_t* handle_;
  Data_Get_Struct(handle, uv_pipe_t, handle_);
  const char* name_ = StringValueCStr(name);
  int retval = uv_pipe_bind(handle_, name_);
  return INT2NUM(retval);
}

// UV_EXTERN void uv_pipe_connect(uv_connect_t* req, uv_pipe_t* handle, const char* name, uv_connect_cb cb)
VALUE foolio_pipe_connect(VALUE self, VALUE req, VALUE handle, VALUE name, VALUE cb) {
  uv_connect_t* req_;
  Data_Get_Struct(req, uv_connect_t, req_);
  uv_pipe_t* handle_;
  Data_Get_Struct(handle, uv_pipe_t, handle_);
  const char* name_ = StringValueCStr(name);
  handle_->data = (void*)callback(cb);
  uv_pipe_connect(req_, handle_, name_, foolio__connect_cb);
  return Qnil;
}

// UV_EXTERN void uv_pipe_pending_instances(uv_pipe_t* handle, int count)
VALUE foolio_pipe_pending_instances(VALUE self, VALUE handle, VALUE count) {
  uv_pipe_t* handle_;
  Data_Get_Struct(handle, uv_pipe_t, handle_);
  int count_ = NUM2INT(count);
  uv_pipe_pending_instances(handle_, count_);
  return Qnil;
}

void Init_foolio_ext(void) {
  klass = rb_define_class_under(rb_define_module("Foolio"), "UV", rb_cObject);
  Method(default_loop, 0);
  Method(loop_new, 0);
  Method(loop_delete, 1);
  Method(close_all, 2);
  Method(close, 2);
  Method(is_active, 1);
  Method(run, 1);
  Method(run_once, 1);
  Method(ip4_addr, 2);
  Method(ip6_addr, 2);
  Method(ip_name, 1);
  Method(port, 1);
  Method(listen, 3);
  Method(accept, 2);
  Method(read_start, 2);
  Method(read_stop, 1);
  Method(read2_start, 2);
  Method(write, 4);
  Method(write2, 5);
  Method(is_readable, 1);
  Method(is_writable, 1);
  Method(is_closing, 1);
  Method(udp_init, 1);
  Method(udp_bind, 3);
  Method(udp_bind6, 3);
  Method(udp_getsockname, 3);
  Method(udp_set_membership, 4);
  Method(udp_set_multicast_loop, 2);
  Method(udp_set_multicast_ttl, 2);
  Method(udp_set_broadcast, 2);
  Method(udp_set_ttl, 2);
  Method(udp_send, 5);
  Method(udp_send6, 5);
  Method(udp_recv_start, 2);
  Method(udp_recv_stop, 1);
  Method(tcp_init, 1);
  Method(tcp_nodelay, 2);
  Method(tcp_keepalive, 3);
  Method(tcp_simultaneous_accepts, 2);
  Method(tcp_bind, 2);
  Method(tcp_bind6, 2);
  Method(tcp_getsockname, 1);
  Method(tcp_getpeername, 3);
  Method(tcp_connect, 4);
  Method(tcp_connect6, 4);
  Method(idle_init, 1);
  Method(idle_start, 2);
  Method(idle_stop, 1);
  Method(timer_init, 1);
  Method(timer_start, 4);
  Method(timer_stop, 1);
  Method(timer_again, 1);
  Method(timer_set_repeat, 2);
  Method(timer_get_repeat, 1);
  Method(fs_event_init, 4);
  Method(pipe_init, 2);
  Method(pipe_open, 2);
  Method(pipe_bind, 2);
  Method(pipe_connect, 4);
  Method(pipe_pending_instances, 2);
}
