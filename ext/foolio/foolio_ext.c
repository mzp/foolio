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
