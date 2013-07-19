#include "catch.hpp"
#include "test-assign-struct-args-if.h"

extern "C" {
    void undo_test_assign_struct_args_if(struct test_assign_struct_args_if *t);
    void test_assign_struct_args_if(struct test_assign_struct_args_if *t);
}

TEST_CASE("simple/assignment in struct in args followed by if", "A destructive assignment w/ if")
{
    struct test_assign_struct_args_if t;
    
    t.x = 17;
    int y = t.x;
    
    test_assign_struct_args_if(&t);
    REQUIRE(t.x != y);
    undo_test_assign_struct_args_if(&t);
    REQUIRE(t.x == y);
}
