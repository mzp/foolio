#include <ruby.h>
#include <uv.h>
#include <stdbool.h>

void *  rb_thread_call_with_gvl(void *(*func)(void *), void *data1);

#define Wrap(expr, f)   Data_Wrap_Struct(klass, 0, (f), (expr))
#define Method(name, n) rb_define_singleton_method(klass, #name, name, n)
#define Get(T, P, FROM) \
  T* P; \
  Data_Get_Struct(FROM, T, P);

struct LoopData {
  bool gvl; // has GVL?
};
// ------------------------------
// Loop
// ------------------------------
VALUE klass;
static VALUE default_loop(VALUE self) {
  return Wrap( uv_default_loop(), 0 );
}

static VALUE run_(void* loop) {
  return INT2NUM(uv_run(loop));
}

static VALUE run(VALUE self, VALUE loop) {
  Get(uv_loop_t, p, loop);
  struct LoopData data = { false };
  p->data = (void*)&data;
  rb_thread_blocking_region(run_, (void*)p, RUBY_UBF_IO, NULL);
}

static VALUE run_once(VALUE self, VALUE loop) {
  Get(uv_loop_t, p, loop);
  struct LoopData data = { true };
  p->data = (void*)&data;
  return INT2NUM(uv_run_once(p));
}

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
    rb_thread_call_with_gvl(callback__apply, (void*)data);
  }
}

void callback_free(struct CallbackData* data) {
  free(data);
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

static void timer_callback(uv_timer_t* handle, int status) {
  VALUE argv[] = { Wrap(handle,0) , INT2NUM(status) };
  callback_apply(handle->loop, handle->data, 2, argv);
}

//UV_EXTERN int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb,
//    int64_t timeout, int64_t repeat);
static VALUE timer_start(VALUE self, VALUE timer, VALUE cb, VALUE timeout, VALUE repeat) {
  Get(uv_timer_t, timer_t, timer);
  int timeout_n = NUM2INT(timeout);
  int repeat_n  = NUM2INT(repeat);
  timer_t->data = (void*)callback(cb);
  return INT2NUM(uv_timer_start(timer_t, timer_callback, timeout_n, repeat_n));
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
