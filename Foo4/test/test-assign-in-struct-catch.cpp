#include "catch.hpp"
#include "test-assign-in-struct.h"

extern "C" {
    struct test_assign_in_struct t;
    void undo_test_assign_in_struct(void);
    void test_assign_in_struct(void);
}

TEST_CASE("simple/assignment in struct", "A destructive assignment")
{
    t.x = 17;
    int y = t.x;
    
    test_assign_in_struct();
    REQUIRE(t.x != y);
    undo_test_assign_in_struct();
    REQUIRE(t.x == y);
}
