#include "catch.hpp"

extern "C" {
    int test_if_x;
    int test_if_x_condition;
    void undo_test_if(void);
    void test_if(void);
}

TEST_CASE("simple/if true", "A simple test if true")
{
    test_if_x = 3;
    test_if_x_condition = 1;
    int y = test_if_x;
    
    test_if();
    REQUIRE(test_if_x != y);
    undo_test_if();
    REQUIRE(test_if_x == y);
}

TEST_CASE("simple/if false", "A simple test if false")
{
    test_if_x = 3;
    test_if_x_condition = 0;
    int y = test_if_x;
    
    test_if();
    REQUIRE(test_if_x == y);
    undo_test_if();
    REQUIRE(test_if_x == y);
}