#include "tracelog.h"

#include "ruby/ruby.h"

#include <assert.h>
#include <time.h>
#include <sys/mman.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

static uint64_t
clock_abs_ns(void)
{
#ifdef __APPLE__
    static mach_timebase_info_data_t timebase;
    uint64_t mach_time;

    if (timebase.denom == 0) mach_timebase_info(&timebase);

    mach_time = mach_absolute_time();
    return mach_time * timebase.numer / timebase.denom;
#else // assuming linux
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC /*CLOCK_THREAD_CPUTIME_ID*/, &ts);
    // assert(ret == 0);
    (void)ret;
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t)(ts.tv_nsec);
#endif
}

typedef struct rb_tracelog_chunk_struct {
    void *data_head;
    size_t len;
    size_t capacity;
    struct rb_tracelog_chunk_struct *next;
} rb_tracelog_chunk_t;

#define TRACELOG_CHUNK_SIZE 32 * 1024 * 1024

void*
tracelog_malloc(size_t size)
{
    void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(ret != MAP_FAILED);
    return ret;
}

const char*
clone_cstr(const char *str)
{
    char* ret;
    size_t len = strlen(str) + 1;
    ret = tracelog_malloc(len);
    strncpy(ret, str, len);
    return ret;
}

rb_tracelog_t* g_tracelog; // FIXME: this should be thread local

static rb_tracelog_chunk_t*
chunk_new(void)
{
    rb_tracelog_chunk_t *chunk = ALLOC(rb_tracelog_chunk_t);
    chunk->data_head = tracelog_malloc(TRACELOG_CHUNK_SIZE);
    chunk->len = 0;
    chunk->capacity = TRACELOG_CHUNK_SIZE;
    chunk->next = NULL;
    return chunk;
}

static void*
chunk_alloc_mem(rb_tracelog_chunk_t* chunk, size_t size)
{
    size_t space_left;
    void* ret;

    assert(!chunk->next);
    assert(chunk->capacity >= chunk->len);
    space_left = chunk->capacity - chunk->len;
    if (space_left < size) {
        rb_tracelog_chunk_t *new_chunk = chunk_new();
        chunk->next = new_chunk;
        g_tracelog->tail_chunk = new_chunk;
        return chunk_alloc_mem(new_chunk, size);
    }

    chunk->len += size;
    ret = chunk->data_head;
    chunk->data_head = (void*)((uintptr_t)chunk->data_head + size);
    return ret;
}

static void*
tracelog_alloc_mem(size_t size)
{
    return chunk_alloc_mem(g_tracelog->tail_chunk, size);
}

static rb_tracelog_event_t*
tracelog_event_alloc()
{
    rb_tracelog_event_t* event = tracelog_alloc_mem(sizeof(rb_tracelog_event_t));
    event->next = NULL;
    g_tracelog->tail_event->next = event;
    g_tracelog->tail_event = event;
    return event;
}

static rb_tracelog_event_t*
intern_tracelog_event_new(const char *name, const char *category, char phase)
{
    rb_tracelog_event_t* event;
    event = tracelog_event_alloc();
    event->name = name;
    event->category = category;
    event->args = NULL;
    event->phase = phase;
    event->timestamp = clock_abs_ns();
    return event;
}

/* name, category must be string literal and not dynamically allocated. */
void
tracelog_event_force_new(const char *name, const char *category, char phase)
{
    if (!g_tracelog) return;

    intern_tracelog_event_new(name, category, phase);
}

typedef struct rb_tracelog_arg_string_literal_struct {
    const char *key;
    const char *val;
} rb_tracelog_arg_string_literal_t;

static VALUE
arg_string_literal_serialize(void* data)
{
    rb_tracelog_arg_string_literal_t *arg = (rb_tracelog_arg_string_literal_t*)data;
    VALUE key = rb_str_new_cstr(arg->key);
    VALUE val = rb_str_new_cstr(arg->val);

    VALUE args = rb_hash_new();
    rb_hash_aset(args, key, val);
    return args;
}

void
rb_tracelog_event_new2(const char *name, const char *category, const char *argname, const char *argdata, char phase)
{
    rb_tracelog_arg_string_literal_t *data;
    rb_tracelog_event_t *event;

    if (!g_tracelog) return;

    data = tracelog_alloc_mem(sizeof(struct rb_tracelog_arg_string_literal_struct));
    data->key = argname;
    data->val = argdata;

    event = intern_tracelog_event_new(name, category, phase);
    event->args = tracelog_alloc_mem(sizeof(struct rb_tracelog_arg_struct));
    event->args->serialize_cb = arg_string_literal_serialize;
    event->args->data = data;
}

void
tracelog_init(void)
{
    rb_tracelog_event_t* event;

    g_tracelog = ALLOC(rb_tracelog_t);
    g_tracelog->head_chunk = chunk_new();
    g_tracelog->tail_chunk = g_tracelog->head_chunk;

    event = tracelog_alloc_mem(sizeof(rb_tracelog_event_t));
    event->name = "TracingStartSentinel";
    event->category = "TraceLog";
    event->args = NULL;
    event->phase = 'I';
    event->timestamp = clock_abs_ns();
    event->next = NULL;

    g_tracelog->head_event = event;
    g_tracelog->tail_event = event;

    g_tracelog->head_category = NULL;
}

static const char DISABLED_BY_DEFAULT_PREFIX[] = "disabled-by-default.";

rb_tracelog_category_t*
tracelog_get_or_create_category(const char *cat_name)
{
    rb_tracelog_category_t *category, *last_category;

    for (category = g_tracelog->head_category; category; category = category->next) {
        if (strcmp(category->cat_name, cat_name) == 0) return category;
    }
    last_category = category;
    
    /* Category w/ cat_name == cat_name was not found. */

    /* Create new category */
    category = (rb_tracelog_category_t*)tracelog_malloc(sizeof(rb_tracelog_category_t));
    category->cat_name = clone_cstr(cat_name);
    category->enabled = (strncmp(DISABLED_BY_DEFAULT_PREFIX, cat_name, sizeof(DISABLED_BY_DEFAULT_PREFIX) - 1) != 0);

    if (last_category) {
        assert(!last_category->next);
        last_category->next = category;
    }
    else {
        g_tracelog->head_category = category; 
    }

    return category;
}

void
tracelog_set_enable(int enable)
{
    fprintf(stderr, "tracelog_set_enable: %d implement!\n", enable);
}
