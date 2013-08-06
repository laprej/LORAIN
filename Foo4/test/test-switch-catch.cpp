#include "catch.hpp"

extern "C" {
    int test_switch_x;
    void undo_test_switch(void);
    void test_switch(void);
}

TEST_CASE("simple/switch", "A trivial switch example")
{
    test_switch_x = 0;
    int y = test_switch_x;
    
    test_switch();
    REQUIRE(test_switch_x != y);
    REQUIRE(test_switch_x == 37);
    undo_test_switch();
    REQUIRE(test_switch_x == y);
    
    test_switch_x = 790;
    y = test_switch_x;
    
    test_switch();
    REQUIRE(test_switch_x != y);
    REQUIRE(test_switch_x == (790 + 134));
    undo_test_switch();
    REQUIRE(test_switch_x == y);
    
    test_switch_x = 13;
    y = test_switch_x;
    
    test_switch();
    REQUIRE(test_switch_x != y);
    REQUIRE(test_switch_x == (13 + 1337));
    undo_test_switch();
    REQUIRE(test_switch_x == y);
}
