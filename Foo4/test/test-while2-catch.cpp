#include "catch.hpp"

extern "C" {
    int i;
    int test_while_x2;
    void undo_test_while2(void);
    void test_while2(void);
}

TEST_CASE("simple/while2", "A simple while-loop test case")
{
    test_while_x2 = 0;
    int y = test_while_x2;
    
    test_while2();
    REQUIRE(test_while_x2 == 5);
    undo_test_while2();
    REQUIRE(test_while_x2 == 0);
}
