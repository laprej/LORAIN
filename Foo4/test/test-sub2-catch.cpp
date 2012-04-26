#include "catch.hpp"

extern "C" {
    int test_sub_x2;
    void undo_test_sub2(void);
    void test_sub2(void);
}

TEST_CASE("simple/sub2", "Test multiple subs")
{
    test_sub_x2 = 3;
    int y = test_sub_x2;
    
    test_sub2();
    REQUIRE(test_sub_x2 != y);
    undo_test_sub2();
    REQUIRE(test_sub_x2 == y);
}