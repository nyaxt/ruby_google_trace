#include "ruby/ruby.h"
#include "ruby/debug.h"

#include "tracelog.h"
#include "cathash.h"

#include <assert.h>
#include <string.h>

static VALUE rb_mTraceJson;
static VALUE rb_cTraceLog;

static VALUE
tracelog_enable()
{
    tracelog_set_enable(1);
}

static VALUE
tracelog_disable()
{
    tracelog_set_enable(0);
}

static const char*
rbstr2tracestr(VALUE val)
{
    /* FIXME: handle \0 */
    char* ret;
    size_t size;

    StringValue(val);
    size = RSTRING_LEN(val) + 1;
    ret = (char*)tracelog_malloc(size);
    strncpy(ret, RSTRING_PTR(val), size);
    return ret;
}

static VALUE
tracelog_create_event(VALUE klass, VALUE v_name, VALUE v_category, VALUE v_phase)
{
    const char *name;
    rb_tracelog_category_t* category;
    char phase;
    rb_tracelog_event_t* event;

    name = rbstr2tracestr(v_name);
    v_category = rb_to_symbol(v_category);
    category = cathash_get_or_create_category(v_category);
    assert(category);

    StringValue(v_phase);
    if (RSTRING_LEN(v_phase) != 1)
        rb_raise(rb_eArgError, "phase must be a char");
    phase = RSTRING_PTR(v_phase)[0];

    tracelog_event_new(name, category, phase);
}

static VALUE
tracelog_to_a()
{
    VALUE ret = rb_ary_new();

    rb_tracelog_event_t* event;
    for (event = g_tracelog->head_event; event; event = event->next) {
        VALUE rb_name = rb_str_new_cstr(event->name);
        VALUE rb_category = rb_str_new_cstr(event->category);
        VALUE rb_args = event->args ? event->args->serialize_cb(event->args->data) : Qnil;
        VALUE rb_phase = rb_str_new(&event->phase, 1);
        VALUE rb_timestamp = ULL2NUM(event->timestamp);

        VALUE rb_event = rb_ary_new_from_args(5, rb_name, rb_category, rb_args, rb_phase, rb_timestamp);
        rb_ary_push(ret, rb_event);
    }

    return ret;
}

static VALUE tracelog_tracepoint_ext = Qnil;
static VALUE tracelog_tracepoint_int = Qnil;
static rb_tracelog_category_t *cat_tp_line = NULL;
static rb_tracelog_category_t *cat_tp_method = NULL;
static rb_tracelog_category_t *cat_tp_thread = NULL;
static rb_tracelog_category_t *cat_tp_gc = NULL;

static void
tracelog_hook(VALUE tracepoint, void* data)
{
    rb_trace_arg_t *targ = rb_tracearg_from_tracepoint(tracepoint);

    rb_event_flag_t event = rb_tracearg_event_flag(targ);
    char phase = 'E';
    /* code based on tracepoint_inspect from ruby/vm_trace.c */
    switch (event) {
      case RUBY_EVENT_LINE:
      case RUBY_EVENT_SPECIFIED_LINE:
        {
            VALUE sym = rb_tracearg_method_id(targ);
            if (NIL_P(sym)) break;
            VALUE lineinfo = rb_sprintf("%"PRIsVALUE":%d in `%"PRIsVALUE"'",
                                        rb_tracearg_path(targ),
                                        FIX2INT(rb_tracearg_lineno(targ)),
                                        sym);
            tracelog_event_new(rbstr2tracestr(lineinfo), cat_tp_line, 'I');
        }
        break;
      case RUBY_EVENT_CALL:
      case RUBY_EVENT_C_CALL:
        phase = 'B';
        /* fall through */
      case RUBY_EVENT_RETURN:
      case RUBY_EVENT_C_RETURN:
        {
            int is_c_call = event & (RUBY_EVENT_C_CALL | RUBY_EVENT_C_RETURN);
            VALUE methodinfo = rb_sprintf("%"PRIsVALUE" %s",
                                          rb_tracearg_method_id(targ),
                                          is_c_call ? "CMethod" : "Method");
            tracelog_event_new(rbstr2tracestr(methodinfo), cat_tp_method, phase);
        }
        break;
      case RUBY_EVENT_B_CALL:
        phase = 'B';
        /* fall through */
      case RUBY_EVENT_B_RETURN:
        tracelog_event_new("*block*", cat_tp_method, phase);
        break;
      case RUBY_EVENT_THREAD_BEGIN:
        phase = 'B';
        /* fall through */
      case RUBY_EVENT_THREAD_END:
        {
            VALUE threadinfo = rb_sprintf("Thread %"PRIsVALUE, rb_tracearg_self(targ));
            tracelog_event_new(rbstr2tracestr(threadinfo), cat_tp_thread, phase);
        }
        break;
      case RUBY_INTERNAL_EVENT_SWITCH:
        {
            VALUE threadinfo = rb_sprintf("ThreadSwitch %"PRIsVALUE, rb_tracearg_self(targ));
            tracelog_event_new(rbstr2tracestr(threadinfo), cat_tp_thread, 'I');
        }
      case RUBY_INTERNAL_EVENT_NEWOBJ:
      case RUBY_INTERNAL_EVENT_FREEOBJ:
        /* ignored */
        /* FIXME: add counter events ??? */
        break;
      /* FIXME: log gc phase as "GC phase" virtual thread */
      case RUBY_INTERNAL_EVENT_GC_START:
        tracelog_event_new("mark", cat_tp_gc, 'B');
        break;
      case RUBY_INTERNAL_EVENT_GC_END_MARK:
        tracelog_event_new("mark", cat_tp_gc, 'E');
        tracelog_event_new("sweep", cat_tp_gc, 'B');
        break;
      case RUBY_INTERNAL_EVENT_GC_END_SWEEP:
        tracelog_event_new("sweep", cat_tp_gc, 'E');
        break;
      case RUBY_INTERNAL_EVENT_GC_ENTER:
        tracelog_event_new("GC", cat_tp_gc, 'B');
        break;
      case RUBY_INTERNAL_EVENT_GC_EXIT:
        tracelog_event_new("GC", cat_tp_gc, 'E');
        break;
      default:
        /* unknown tracepoint */
        fprintf(stderr, "unknown tracepoint! %x\n", event);
    }
}

static VALUE
tracelog_hook_tracepoint_enable(VALUE self)
{
    if (!RTEST(tracelog_tracepoint_ext))
    {
        assert(!RTEST(tracelog_tracepoint_int));
        tracelog_tracepoint_ext = rb_tracepoint_new(Qnil, RUBY_EVENT_TRACEPOINT_ALL, tracelog_hook, NULL);
        tracelog_tracepoint_int = rb_tracepoint_new(Qnil, RUBY_INTERNAL_EVENT_MASK, tracelog_hook, NULL);
        rb_iv_set(rb_cTraceLog, "tracepoint_ext", tracelog_tracepoint_ext);
        rb_iv_set(rb_cTraceLog, "tracepoint_int", tracelog_tracepoint_int);
    }

    assert(RTEST(tracelog_tracepoint_ext));
    rb_tracepoint_enable(tracelog_tracepoint_ext);
    assert(RTEST(tracelog_tracepoint_int));
    rb_tracepoint_enable(tracelog_tracepoint_int);

    return self;
}

static VALUE
tracelog_hook_tracepoint_disable(VALUE self)
{
    if (RTEST(tracelog_tracepoint_ext))
        rb_tracepoint_disable(tracelog_tracepoint_ext);
    if (RTEST(tracelog_tracepoint_int))
        rb_tracepoint_disable(tracelog_tracepoint_int);

    return self;
}

void
Init_ext_tracejson(void)
{
    tracelog_init();
    cathash_init();

    rb_mTraceJson = rb_define_module("TraceJson");
    rb_cTraceLog = rb_define_class_under(rb_mTraceJson, "TraceLog", rb_cObject);
    rb_define_singleton_method(rb_cTraceLog, "enable", tracelog_enable, 0);
    rb_define_singleton_method(rb_cTraceLog, "disable", tracelog_disable, 0);
    rb_define_singleton_method(rb_cTraceLog, "create_event", tracelog_create_event, 3);
    rb_define_singleton_method(rb_cTraceLog, "hook_tracepoint_enable", tracelog_hook_tracepoint_enable, 0);
    rb_define_singleton_method(rb_cTraceLog, "hook_tracepoint_disable", tracelog_hook_tracepoint_disable, 0);

    rb_define_singleton_method(rb_cTraceLog, "to_a", tracelog_to_a, 0);

    cat_tp_line = tracelog_get_or_create_category("tp.line");
    cat_tp_method = tracelog_get_or_create_category("tp.method");
    cat_tp_thread = tracelog_get_or_create_category("tp.thread");
    cat_tp_gc = tracelog_get_or_create_category("tp.gc");

    /*
    rb_cEvent = rb_define_class_under(rb_cTraceLog, "Event", rb_cObject);
    rb_define_singleton_method(rb_cEvent, "new", event_new);

    rb_add_event_hook(tracelog_event_hook, RUBY_EVENT_ALL, 
    */
}
