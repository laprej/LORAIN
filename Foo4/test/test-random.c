int test_random_x;
int test_random_y;

void undo_test_random(void);

void test_random(void)
{
    test_random_y = 42;
    test_random_y = test_random_y * 2;
    test_random_x = test_random_x * 3;
    test_random_x = test_random_x - test_random_y;
    
    if (test_random_x > 0) {
        test_random_x = test_random_x + 1;
    }
    else {
        test_random_x = test_random_x - 1;
    }
}