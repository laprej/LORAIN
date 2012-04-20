#include "catch.hpp"

extern "C" {
    int test_add_x2;
    void undo_test_add2(void);
    void test_add2(void);
}

TEST_CASE("simple/add2", "Test multiple additions")
{
    test_add_x2 = 3;
    int y = test_add_x2;
    
    test_add2();
    undo_test_add2();
    REQUIRE(test_add_x2 == y);
}