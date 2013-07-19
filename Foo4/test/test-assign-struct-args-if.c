#include "test-assign-struct-args-if.h"

void undo_test_assign_struct_args_if(struct test_assign_struct_args_if *t);

void test_assign_struct_args_if(struct test_assign_struct_args_if *t)
{
    t->x = 3;
    
    if (t->x > 2) {
        t->x *= 2;
    }
}
