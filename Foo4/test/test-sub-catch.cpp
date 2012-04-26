#include "catch.hpp"

extern "C" {
    int test_sub_x;
    void undo_test_sub(void);
    void test_sub(void);
}

TEST_CASE("simple/sub", "A simple test case where we subtract a constant")
{
    test_sub_x = 3;
    int y = test_sub_x;
    
    test_sub();
    undo_test_sub();
    REQUIRE(test_sub_x == y);
}