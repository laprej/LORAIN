#include "catch.hpp"

extern "C" {
    int test_random_x;
    void undo_test_random(void);
    void test_random(void);
}

TEST_CASE("simple/random", "A random test case")
{
    test_random_x = 3;
    int y = test_random_x;
    
    test_random();
    REQUIRE(test_random_x != y);
    undo_test_random();
    REQUIRE(test_random_x == y);
}
