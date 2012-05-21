#include "catch.hpp"

extern "C" {
    int test_if_else_x;
    int test_if_else_x_condition;
    void undo_test_if_else(void);
    void test_if_else(void);
}

TEST_CASE("simple/if-else true", "A simple test if true")
{
    test_if_else_x = 3;
    test_if_else_x_condition = 1;
    int y = test_if_else_x;
    
    test_if_else();
    REQUIRE(test_if_else_x != y);
    undo_test_if_else();
    REQUIRE(test_if_else_x == y);
}

TEST_CASE("simple/if-else false", "A simple test if false")
{
    test_if_else_x = 3;
    test_if_else_x_condition = 0;
    int y = test_if_else_x;
    
    test_if_else();
    REQUIRE(test_if_else_x != y);
    undo_test_if_else();
    REQUIRE(test_if_else_x == y);
}