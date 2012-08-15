#include <ruby.h>
#include <uv.h>
#define Wrap(expr, f)   Data_Wrap_Struct(klass, 0, (f), (expr))
#define Method(name, n) rb_define_singleton_method(klass, #name, name, n)
#define Get(T, P, FROM) \
  T* P; \
  Data_Get_Struct(FROM, T, P);

VALUE klass;
static VALUE default_loop(VALUE self) {
  return Wrap( uv_default_loop(), 0 );
}

static VALUE run_blocking(void* loop) {
  return INT2NUM(uv_run(loop));
}

static VALUE run(VALUE self, VALUE loop) {
  Get(uv_loop_t, p, loop);
  rb_thread_blocking_region(run_blocking, (void*)p, RUBY_UBF_IO, NULL);
}

static VALUE run_once(VALUE self, VALUE loop) {
  Get(uv_loop_t, p, loop);
  return INT2NUM(uv_run_once(p));
}

// ------------
// timer
// ------------
//UV_EXTERN int uv_timer_init(uv_loop_t*, uv_timer_t* timer);
static VALUE timer_init(VALUE self, VALUE loop) {
  uv_timer_t* timer = malloc(sizeof(uv_timer_t));
  Get(uv_loop_t, loop_data, loop);
  if( uv_timer_init(loop_data, timer) == 0) {
    return Wrap(timer, free);
  } else {
    return Qnil;
  }
}

void *  rb_thread_call_with_gvl(void *(*func)(void *), void *data1);

static void* timer_cb_blocking(void* handle) {
  uv_timer_t* p = (uv_timer_t*)handle;
  rb_funcall((VALUE)p->data, rb_intern("call"), 1, INT2NUM(0));
  return 0;
}

static void timer_cb(uv_timer_t* handle, int status) {
  rb_thread_call_with_gvl(timer_cb_blocking, handle);
}

//UV_EXTERN int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb,
//    int64_t timeout, int64_t repeat);
static VALUE timer_start(VALUE self, VALUE timer, VALUE cb, VALUE timeout, VALUE repeat) {
  Get(uv_timer_t, timer_data, timer);
  int timeout_n = NUM2INT(timeout);
  int repeat_n  = NUM2INT(repeat);
  timer_data->data = (void*)cb;
  return INT2NUM(uv_timer_start(timer_data, timer_cb, timeout_n, repeat_n));
}

// UV_EXTERN int uv_timer_stop(uv_timer_t* timer);
static VALUE timer_stop(VALUE self, VALUE timer) {
  Get(uv_timer_t, timer_data, timer);
  return INT2FIX(uv_timer_stop(timer_data));
}

// UV_EXTERN int uv_timer_again(uv_timer_t* timer);
static VALUE timer_again(VALUE self, VALUE timer) {
  Get(uv_timer_t, timer_data, timer);
  return INT2FIX(uv_timer_again(timer_data));
}
/*
UV_EXTERN int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb,
    int64_t timeout, int64_t repeat);
*/

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
