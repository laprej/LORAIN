#include "catch.hpp"

extern "C" {
    int i;
    int test_array_x[10];
    void undo_test_array(void);
    void test_array(void);
}

TEST_CASE("simple/array", "A simple test case where we use arrays")
{
    for (int j = 0; j < 10; j++) {
        test_array_x[j] = 0;
    }
    
    int copy_array[10];
    for (int j = 0; j < 10; j++) {
        copy_array[j] = test_array_x[j];
    }
    
    test_array();
    
    for (int j = 0; j < 10; j++) {
        REQUIRE(test_array_x[j] != copy_array[j]);
    }
    
    undo_test_array();
    for (int j = 0; j < 10; j++) {
        REQUIRE(test_array_x[j] == copy_array[x]);
    }
}
