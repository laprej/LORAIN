#include "catch.hpp"

extern "C" {
    int test_assign_x;
    void undo_test_assign(void);
    void test_assign(void);
}

TEST_CASE("simple/assignment", "A destructive assignment")
{
    test_assign_x = 17;
    int y = test_assign_x;
    
    test_assign();
    REQUIRE(test_assign_x != y);
    undo_test_assign();
    REQUIRE(test_assign_x == y);
}
