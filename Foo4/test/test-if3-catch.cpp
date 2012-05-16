#include "catch.hpp"

extern "C" {
    int test_if_x3_1;
    int test_if_x3_2;
    int test_if_x_condition3_1;
    int test_if_x_condition3_2;
    void undo_test_if3(void);
    void test_if3(void);
}

TEST_CASE("simple/nested if TT", "A simple test if TT")
{
    test_if_x3_1 = 3;
    test_if_x3_2 = 8;
    test_if_x_condition3_1 = 1;
    test_if_x_condition3_2 = 1;
    int y1 = test_if_x3_1;
    int y2 = test_if_x3_2;
    
    test_if3();
    REQUIRE(test_if_x3_1 != y1);
    REQUIRE(test_if_x3_2 != y2);
    undo_test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
}

TEST_CASE("simple/nested if TF", "A simple test if TF")
{
    test_if_x3_1 = 3;
    test_if_x3_2 = 8;
    test_if_x_condition3_1 = 1;
    test_if_x_condition3_2 = 0;
    int y1 = test_if_x3_1;
    int y2 = test_if_x3_2;
    
    test_if3();
    REQUIRE(test_if_x3_1 != y1);
    REQUIRE(test_if_x3_2 == y2);
    undo_test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
}

TEST_CASE("simple/nested if FT", "A simple test if FT")
{
    test_if_x3_1 = 3;
    test_if_x3_2 = 8;
    test_if_x_condition3_1 = 0;
    test_if_x_condition3_2 = 1;
    int y1 = test_if_x3_1;
    int y2 = test_if_x3_2;
    
    test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
    undo_test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
}

TEST_CASE("simple/nested if FF", "A simple test if FF")
{
    test_if_x3_1 = 3;
    test_if_x3_2 = 8;
    test_if_x_condition3_1 = 0;
    test_if_x_condition3_2 = 0;
    int y1 = test_if_x3_1;
    int y2 = test_if_x3_2;
    
    test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
    undo_test_if3();
    REQUIRE(test_if_x3_1 == y1);
    REQUIRE(test_if_x3_2 == y2);
}
