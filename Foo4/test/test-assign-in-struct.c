#include "test-assign-in-struct.h"

struct test_assign_in_struct t;

void undo_test_assign_in_struct(void);

void test_assign_in_struct(void)
{
    t.x = 3;
}
