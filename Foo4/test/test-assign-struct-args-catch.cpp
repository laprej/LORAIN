#include "catch.hpp"
#include "test-assign-struct-args.h"

extern "C" {
    void undo_test_assign_struct_args(struct test_assign_struct_args *t);
    void test_assign_struct_args(struct test_assign_struct_args *t)
}

TEST_CASE("simple/assignment in struct in args", "A destructive assignment")
{
    struct test_assign_struct_args t;
    
    t.x = 17;
    int y = t.x;
    
    test_assign_struct_args(&t);
    REQUIRE(t.x != y);
    undo_test_assign_struct_args(&t);
    REQUIRE(t.x == y);
}
