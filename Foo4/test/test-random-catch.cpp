#include "catch.hpp"
#include <iostream>

extern "C" {
    int test_random_y;
    int test_random_x;
    void undo_test_random(void);
    void test_random(void);
}

TEST_CASE("simple/random", "A random test case")
{
    test_random_x = 3;
    test_random_y = 0;

    int z = test_random_x;
    
    test_random();
    REQUIRE(test_random_x != z);
    undo_test_random();
    REQUIRE(test_random_x == z);
}
