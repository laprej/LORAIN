#include "test-augment-struct.h"

void undo_test_augment_struct(struct test_augment_struct_from *from,
                              struct test_augment_struct_to *to);

void test_augment_struct(struct test_augment_struct_from *from,
                         struct test_augment_struct_to *to)
{
    from->x++;
}
