#include "test-assign-struct-args.h"

void undo_test_assign_struct_args(struct test_assign_struct_args *t);

void test_assign_struct_args(struct test_assign_struct_args *t)
{
    t->x = 3;
}
