int *test_ptr_x;

void undo_test_ptr(void);

void test_ptr(void)
{
    *test_ptr_x = *test_ptr_x + 2;
}