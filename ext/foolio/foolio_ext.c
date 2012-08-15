#include <ruby.h>
#include <uv.h>
#include <stdbool.h>

void *  rb_thread_call_with_gvl(void *(*func)(void *), void *data1);

#define Wrap(expr, f)   Data_Wrap_Struct(klass, 0, (f), (expr))
#define Method(name, n) rb_define_singleton_method(klass, #name, name, n)
#define Decl(T) \
  uv_##T##_t * T##_p; \
  Data_Get_Struct(T, uv_##T##_t, T##_p)

#define InitHandle(T)       \
  uv_##T##_t * handle = malloc(sizeof(uv_##T##_t));  \
  Decl(loop);      \
  if( uv_##T##_init(loop_p, handle) == 0) {   \
    return Wrap(handle, free);    \
  } else {                        \
    return Qnil;                  \
  }                               \


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
};

struct CallbackData* callback(VALUE cb) {
  struct CallbackData* data = malloc(sizeof(struct CallbackData));
  data->cb = cb;
  data->argc = 0;
  data->argv = NULL;
  return data;
}

void* callback__apply(void* p) {
  struct CallbackData* data = (struct CallbackData*)p;
  ID call = rb_intern("call");
  rb_funcall2(data->cb, call, data->argc, data->argv);
  return NULL;
}

void callback_apply(uv_loop_t* loop, struct CallbackData* data, int argc, VALUE argv[]) {
  struct LoopData* p = loop->data;
  data->argc = argc;
  data->argv = argv;
  if(p->gvl) {
    callback__apply((void*)data);
  } else {
    // All callback function are run without gvl.
    // But we need GVL to call Ruby methods.
    rb_thread_call_with_gvl(callback__apply, (void*)data);
  }
}

void callback_free(struct CallbackData* data) {
  free(data);
}

// ------------------------------
// Loop
// ------------------------------
static VALUE default_loop(VALUE self) {
  return Wrap( uv_default_loop(), 0 );
}

static VALUE loop_new(VALUE self){
  return Wrap( uv_loop_new(), 0);
}

static VALUE loop_delete(VALUE self, VALUE loop) {
  Decl(loop);
  uv_loop_delete(loop_p);
  return Qnil;
}

// ------------------------------
// run
// ------------------------------
static VALUE run_(void* loop) {
  return INT2NUM(uv_run(loop));
}

static VALUE run(VALUE self, VALUE loop) {
  Decl(loop);
  struct LoopData data = { false };
  loop_p->data = (void*)&data;
  rb_thread_blocking_region(run_, (void*)loop_p, RUBY_UBF_IO, NULL);
}

static VALUE run_once(VALUE self, VALUE loop) {
  Decl(loop);
  struct LoopData data = { true };
  loop_p->data = (void*)&data;
  return INT2NUM(uv_run_once(loop_p));
}

static VALUE ip4_addr(VALUE ip, VALUE port) {
  struct sockaddr_in* p = malloc(sizeof(struct sockaddr_in));
  *p = uv_ip4_addr((const char*)StringValueCStr(ip), NUM2INT(port));
  return Wrap(p, free);
}

static VALUE ip6_addr(VALUE ip, VALUE port) {
  struct sockaddr_in6* p = malloc(sizeof(struct sockaddr_in6));
  *p = uv_ip6_addr((const char*)StringValueCStr(ip), NUM2INT(port));
  return Wrap(p, free);
}

//UV_EXTERN struct sockaddr_in uv_ip4_addr(const char* ip, int port);
//UV_EXTERN struct sockaddr_in6 uv_ip6_addr(const char* ip, int port);

/*
UV_EXTERN int uv_tcp_init(uv_loop_t*, uv_tcp_t* handle);
UV_EXTERN int uv_tcp_nodelay(uv_tcp_t* handle, int enable);
UV_EXTERN int uv_tcp_keepalive(uv_tcp_t* handle, int enable,unsigned int delay);

UV_EXTERN int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable);
UV_EXTERN int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in);
UV_EXTERN int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6);
UV_EXTERN int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen);
UV_EXTERN int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen);
UV_EXTERN int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in address, uv_connect_cb cb);
UV_EXTERN int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in6 address, uv_connect_cb cb);
*/
static VALUE tcp_init(uv_loop_t* loop) {
  InitHandle(tcp);
}

// UV_EXTERN int uv_tcp_nodelay(uv_tcp_t* handle, int enable)
static VALUE tcp_nodelay(VALUE self, VALUE handle, VALUE enable) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  int retval = uv_tcp_nodelay(handle_, enable_);
  return Qnil;
}

// UV_EXTERN int uv_tcp_keepalive(uv_tcp_t* handle, int enable,unsigned int delay)
static VALUE tcp_keepalive(VALUE self, VALUE handle, VALUE enable, VALUE delay) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  unsigned int delay_ = NUM2UINT(delay);
  int retval = uv_tcp_keepalive(handle_, enable_, delay_);
  return INT2NUM(retval);
}

// UV_EXTERN int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable)
static VALUE tcp_simultaneous_accepts(VALUE self, VALUE handle, VALUE enable) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  int enable_ = NUM2INT(enable);
  int retval = uv_tcp_simultaneous_accepts(handle_, enable_);
  return INT2NUM(retval);
}


// UV_EXTERN int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in)
static VALUE tcp_bind(VALUE self, VALUE handle, VALUE sockaddr_in) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in* sockaddr_in_;
  Data_Get_Struct(sockaddr_in, struct sockaddr_in, sockaddr_in_);
  int retval = uv_tcp_bind(handle_, *sockaddr_in_);
  return Qnil;
}

// UV_EXTERN int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6)
static VALUE tcp_bind6(VALUE self, VALUE handle, VALUE sockaddr_in6) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in6* sockaddr_in6_;
  Data_Get_Struct(sockaddr_in6, struct sockaddr_in6, sockaddr_in6_);

  int retval = uv_tcp_bind6(handle_, *sockaddr_in6_);
  return Qnil;
}

// UV_EXTERN int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,    int* namelen)
static VALUE tcp_getsockname(VALUE self, VALUE handle) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr* name_ = malloc(sizeof(struct sockaddr));
  int size;
  int retval = uv_tcp_getsockname(handle_, name_, &size);
  return Wrap(name_, free);
}

// UV_EXTERN int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,    int* namelen)
static VALUE tcp_getpeername(VALUE self, VALUE handle, VALUE name, VALUE namelen) {
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr* name_ = malloc(sizeof(struct sockaddr));
  int size;
  int retval = uv_tcp_getpeername(handle_, name_, &size);
  return Wrap(name_, free);
}

static void connect__callback(uv_connect_t* req, int status) {
  VALUE argv[] = { Wrap(req,0), INT2NUM(status) };
  callback_apply(req->handle->loop, req->data, 2, argv);
}
// UV_EXTERN int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle,    struct sockaddr_in address, uv_connect_cb cb)
static VALUE tcp_connect(VALUE self, VALUE req, VALUE handle, VALUE address, VALUE cb) {
  uv_connect_t* req_;
  Data_Get_Struct(req, uv_connect_t, req_);
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in* address_;
  Data_Get_Struct(address, struct sockaddr_in, address_);
  req_->data = (void*)callback(cb);
  int retval = uv_tcp_connect(req_, handle_, *address_, connect__callback);
  return Qnil;
}

// UV_EXTERN int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,    struct sockaddr_in6 address, uv_connect_cb cb)
static VALUE tcp_connect6(VALUE self, VALUE req, VALUE handle, VALUE address, VALUE cb) {
  uv_connect_t* req_;
  Data_Get_Struct(req, uv_connect_t, req_);
  uv_tcp_t* handle_;
  Data_Get_Struct(handle, uv_tcp_t, handle_);
  struct sockaddr_in6* address_;
  Data_Get_Struct(address, struct sockaddr_in6, address_);
  req_->data = (void*)callback(cb);
  int retval = uv_tcp_connect6(req_, handle_, *address_, connect__callback);
  return Qnil;
}



/*
  UV_EXTERN int uv_idle_init(uv_loop_t*, uv_idle_t* idle);
  UV_EXTERN int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);
  UV_EXTERN int uv_idle_stop(uv_idle_t* idle);
*/
static VALUE idle_init(VALUE self, VALUE loop) {
  InitHandle(idle);
}

static void idle_callback(uv_idle_t* handle, int status) {
  VALUE argv[] = { Wrap(handle,0) , INT2NUM(status) };
  callback_apply(handle->loop, handle->data, 2, argv);
}

static VALUE idle_start(VALUE self, VALUE idle, VALUE cb) {
  Decl(idle);
  idle_p->data = callback(cb);
  uv_idle_start(idle_p, idle_callback);
}

static VALUE idle_stop(VALUE self, VALUE idle) {
  Decl(idle);
  callback_free(idle_p->data);
  uv_idle_stop(idle_p);
}

/**
   UV_EXTERN int uv_timer_init(uv_loop_t*, uv_timer_t* timer);
   UV_EXTERN int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb,
   UV_EXTERN int uv_timer_stop(uv_timer_t* timer);
   UV_EXTERN int uv_timer_again(uv_timer_t* timer);
   UV_EXTERN void uv_timer_set_repeat(uv_timer_t* timer, int64_t repeat);
   UV_EXTERN int64_t uv_timer_get_repeat(uv_timer_t* timer);
 */
static VALUE timer_init(VALUE self, VALUE loop) {
  InitHandle(timer);
}

static void timer_callback(uv_timer_t* handle, int status) {
  VALUE argv[] = { Wrap(handle,0) , INT2NUM(status) };
  callback_apply(handle->loop, handle->data, 2, argv);
}

static VALUE timer_start(VALUE self, VALUE timer, VALUE cb, VALUE timeout, VALUE repeat) {
  Decl(timer);
  timer_p->data = (void*)callback(cb);
  int retval =
    uv_timer_start(timer_p, timer_callback, NUM2INT(timeout), NUM2INT(repeat));
  return INT2NUM(retval);
}

static VALUE timer_stop(VALUE self, VALUE timer) {
  Decl(timer);
  callback_free(timer_p->data);
  return INT2FIX(uv_timer_stop(timer_p));
}

static VALUE timer_again(VALUE self, VALUE timer) {
  Decl(timer);
  return INT2FIX(uv_timer_again(timer_p));
}

static VALUE timer_set_repeat(VALUE timer, VALUE repeat) {
  Decl(timer);
  int64_t repeat_n = NUM2LONG(repeat);
  uv_timer_set_repeat(timer_p, repeat);
  return Qnil;
}

static VALUE timer_get_repeat(VALUE timer) {
  Decl(timer);
  int64_t retval = uv_timer_get_repeat(timer_p);
  return LONG2NUM(retval);
}


void Init_foolio_ext(void) {
  klass = rb_define_class_under(rb_define_module("Foolio"),
                                "UV",
                                rb_cObject);
  Method( default_loop, 0);
  Method( run, 1 );
  Method( run_once, 1 );
  Method( timer_init, 1);
  Method( timer_start, 4);
  Method( timer_stop, 1);
  Method( timer_again, 1);
}
