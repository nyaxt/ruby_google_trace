#include "cathash.h"

#include "tracelog.h"

#include "ruby/ruby.h"
#include "ruby/st.h"

#include <assert.h>

static int
rb_sym_cmp(VALUE a, VALUE b)
{
    assert(SYMBOL_P(a));
    assert(SYMBOL_P(b));
    return a != b;
}

static st_index_t
rb_sym_hash(VALUE a)
{
    assert(SYMBOL_P(a));
    return SYM2ID(a);
}

static const struct st_hash_type rb_sym_to_category_hash = {
    rb_sym_cmp,
    rb_sym_hash
};

static st_table *rb_sym_to_category;

void
cathash_init()
{
    rb_sym_to_category = st_init_table(&rb_sym_to_category_hash);
}

rb_tracelog_category_t*
cathash_get_or_create_category(VALUE v_category)
{
    st_data_t data;
    rb_tracelog_category_t* category;
    VALUE v_category_str;
    
    assert(SYMBOL_P(v_category));
    if (st_lookup(rb_sym_to_category, (st_data_t)v_category, &data)) {
        category = (rb_tracelog_category_t*)data; 
        return category;
    }

    /* Not found in cache. */
    SYM2ID(v_category); /* pin down symbol */
    v_category_str = rb_sym2str(v_category);
    category = tracelog_get_or_create_category(StringValueCStr(v_category_str));
    st_add_direct(rb_sym_to_category, (st_data_t)v_category, (st_data_t)category);
    return category;
}
