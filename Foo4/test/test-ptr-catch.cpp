#include "catch.hpp"

extern "C" {
    int *test_ptr_x;
    void undo_test_ptr(void);
    void test_ptr(void);
}

TEST_CASE("simple/ptr", "Basic pointer functionality")
{
    test_ptr_x = (int *)malloc(sizeof(int));
    *test_ptr_x = 0;
    
    int y = *test_ptr_x;
    
    test_ptr();
    REQUIRE(*test_ptr_x != y);
    undo_test_ptr();
    REQUIRE(*test_ptr_x == y);
}