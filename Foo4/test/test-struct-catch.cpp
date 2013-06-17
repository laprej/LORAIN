#include "catch.hpp"
#include "test-struct.h"

extern "C" {
    struct test_struct s;
    void undo_test_struct(void);
    void test_struct(void);
}

TEST_CASE("struct", "Simple struct stuff")
{
    s.test_x = 5;
    
    int y = s.test_x;
    
    test_struct();
    REQUIRE(s.test_x != y);
    undo_test_struct();
    REQUIRE(s.test_x == y);
}
