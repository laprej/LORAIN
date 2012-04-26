int test_if_x;
int test_if_x_condition;

void undo_test_if(void);

void test_if(void)
{
    if (test_if_x_condition) {
        test_if_x = test_if_x + 1;
    }
}
