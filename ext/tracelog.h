#ifndef _tracelog_h_
#define _tracelog_h_

#include "ruby/ruby.h"

typedef struct rb_tracelog_category_struct {
    int enabled;
    const char *cat_name;
    struct rb_tracelog_category_struct *next;
} rb_tracelog_category_t;

typedef struct rb_tracelog_arg_struct {
    VALUE (*serialize_cb)(void*);
    void* data;
} rb_tracelog_args_t;

typedef struct rb_tracelog_event_struct {
    const char *name;
    const char *category;
    struct rb_tracelog_arg_struct *args;
    char phase;
    uint64_t timestamp;
    struct rb_tracelog_event_struct *next;
} rb_tracelog_event_t;

typedef struct rb_tracelog_struct {
    struct rb_tracelog_chunk_struct *head_chunk;
    struct rb_tracelog_chunk_struct *tail_chunk;

    struct rb_tracelog_event_struct *head_event;
    struct rb_tracelog_event_struct *tail_event;

    struct rb_tracelog_category_struct *head_category;
} rb_tracelog_t;
extern rb_tracelog_t* g_tracelog; // FIXME: this should be thread local

void tracelog_init(void);
rb_tracelog_category_t* tracelog_get_or_create_category(const char *category_name);

void tracelog_set_enable(int);

void tracelog_event_force_new(const char *name, const char *category, char phase);

#define tracelog_event_new(name, category, phase) \
    do { \
        if (category->enabled) \
            tracelog_event_force_new(name, category->cat_name, phase); \
    } while(0)

void* tracelog_malloc(size_t size);

#endif /* _tracelog_h_ */
