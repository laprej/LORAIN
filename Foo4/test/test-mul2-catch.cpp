#include "catch.hpp"

extern "C" {
    int test_mul_x2;
    void undo_test_mul2(void);
    void test_mul2(void);
}

TEST_CASE("simple/mul2", "Test multiple muls")
{
    test_mul_x2 = 3;
    int y = test_mul_x2;
    
    test_mul2();
    REQUIRE(test_mul_x2 != y);
    undo_test_mul2();
    REQUIRE(test_mul_x2 == y);
}