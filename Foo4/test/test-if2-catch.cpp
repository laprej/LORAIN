#include "catch.hpp"

extern "C" {
    int test_if_x2_1;
    int test_if_x2_2;
    int test_if_x_condition2_1;
    int test_if_x_condition2_2;
    void undo_test_if2(void);
    void test_if2(void);
}

TEST_CASE("simple/if TT", "A simple test if TT")
{
    test_if_x2_1 = 3;
    test_if_x2_2 = 8;
    test_if_x_condition2_1 = 1;
    test_if_x_condition2_2 = 1;
    int y1 = test_if_x2_1;
    int y2 = test_if_x2_2;
    
    test_if2();
    REQUIRE(test_if_x2_1 != y1);
    REQUIRE(test_if_x2_2 != y2);
    undo_test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 == y2);
}

TEST_CASE("simple/if TF", "A simple test if TF")
{
    test_if_x2_1 = 3;
    test_if_x2_2 = 8;
    test_if_x_condition2_1 = 1;
    test_if_x_condition2_2 = 0;
    int y1 = test_if_x2_1;
    int y2 = test_if_x2_2;
    
    test_if2();
    REQUIRE(test_if_x2_1 != y1);
    REQUIRE(test_if_x2_2 == y2);
    undo_test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 == y2);
}

TEST_CASE("simple/if FT", "A simple test if FT")
{
    test_if_x2_1 = 3;
    test_if_x2_2 = 8;
    test_if_x_condition2_1 = 0;
    test_if_x_condition2_2 = 1;
    int y1 = test_if_x2_1;
    int y2 = test_if_x2_2;
    
    test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 != y2);
    undo_test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 == y2);
}

TEST_CASE("simple/if FF", "A simple test if FF")
{
    test_if_x2_1 = 3;
    test_if_x2_2 = 8;
    test_if_x_condition2_1 = 0;
    test_if_x_condition2_2 = 0;
    int y1 = test_if_x2_1;
    int y2 = test_if_x2_2;
    
    test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 == y2);
    undo_test_if2();
    REQUIRE(test_if_x2_1 == y1);
    REQUIRE(test_if_x2_2 == y2);
}
