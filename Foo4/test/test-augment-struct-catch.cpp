#include "catch.hpp"
#include "test-augment-struct.h"

extern "C" {
    void undo_test_augment_struct(struct test_augment_struct_from *from,
                                  struct test_augment_struct_to *to);
    
    void test_augment_struct(struct test_augment_struct_from *from,
                             struct test_augment_struct_to *to);
}

TEST_CASE("simple/augment struct", "Augment one of our structs")
{
    struct test_augment_struct_from from;
    struct test_augment_struct_to to;
    
    from.x = 42;
    int y = from.x;
    
    test_augment_struct(&from, &to);
    REQUIRE(t.x != y);
    
}
