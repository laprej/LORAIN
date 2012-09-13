#include "catch.hpp"

extern "C" {
    int test_for_x;
    void undo_test_for(void);
    void test_for(void);
}

TEST_CASE("simple/for", "A simple for loop")
{
    test_for_x = 0;
    int y = test_for_x;
    
    test_for();
    REQUIRE(test_for_x != y);
    undo_test_for();
    REQUIRE(test_for_x == y);
}
