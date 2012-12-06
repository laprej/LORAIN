int test_random_x;

void undo_test_random(void);

void test_random(void)
{
    int y = 42;
    y = y * 2;
    test_random_x = test_random_x * 3;
    test_random_x = test_random_x - y;
    
    if (test_random_x > 0) {
        test_random_x = test_random_x + 1;
    }
    else {
        test_random_x = test_random_x - 1;
    }
}