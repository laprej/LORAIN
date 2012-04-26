int test_add_x2;

void undo_test_add2(void);

void test_add2(void)
{
    test_add_x2 = test_add_x2 + 1;
    test_add_x2 = test_add_x2 + 7;
}
