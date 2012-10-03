#include "catch.hpp"

extern "C" {
    int test_add_x;
    void undo_test_add(void);
    void test_add(void);
}

TEST_CASE("simple/add", "A simple test case where we add a constant")
{
    test_add_x = 3;
    int y = test_add_x;
    
    test_add();
    REQUIRE(test_add_x != y);
    undo_test_add();
    REQUIRE(test_add_x == y);
}
