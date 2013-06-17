#include "test-struct.h"

struct test_struct s;

void undo_test_struct(void);

void test_struct(void)
{
    s.test_x = s.test_x + 3;
}
