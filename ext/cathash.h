#ifndef _cathash_h_
#define _cathash_h_

#include "tracelog.h"

#include "ruby/ruby.h"

void cathash_init();
rb_tracelog_category_t* cathash_get_or_create_category(VALUE v_category);

#endif /* _cathash_h_ */
