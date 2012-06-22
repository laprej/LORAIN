#include "catch.hpp"

extern "C" {
    int test_while_x;
    void undo_test_while(void);
    void test_while(void);
}

TEST_CASE("simple/while", "A simple while-loop test case")
{
    test_while_x = 0;
    int y = test_while_x;
    
    test_while();
    REQUIRE(test_while_x == 5);
    undo_test_while();
    REQUIRE(test_while_x == 0);
}