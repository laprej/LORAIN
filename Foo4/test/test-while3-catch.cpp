#include "catch.hpp"

extern "C" {
    int j;
    int test_while_x3;
    void undo_test_while3(void);
    void test_while3(void);
}

TEST_CASE("simple/while3", "A simple while-loop test case")
{
    test_while_x3 = 0;
    int y = test_while_x3;
    
    test_while3();
    REQUIRE(test_while_x3 == 5);
    undo_test_while3();
    REQUIRE(test_while_x3 == 0);
}
