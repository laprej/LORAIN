#include "catch.hpp"

extern "C" {
    int test_mul_x;
    void undo_test_mul(void);
    void test_mul(void);
}

TEST_CASE("simple/mul", "A simple test case where we mul a constant")
{
    test_mul_x = 3;
    int y = test_mul_x;
    
    test_mul();
    REQUIRE(test_mul_x != y);
    undo_test_mul();
    REQUIRE(test_mul_x == y);
}
