int test_add_x;

void undo_test_add2(void);

void test_add2(void)
{
    test_add_x = test_add_x + 1;
    test_add_x = test_add_x + 7;
}
