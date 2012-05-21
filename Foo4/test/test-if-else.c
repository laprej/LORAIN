int test_if_else_x;
int test_if_else_x_condition;

void undo_test_if_else(void);

void test_if_else(void)
{
    if (test_if_else_x_condition) {
        test_if_else_x = test_if_else_x + 1;
    }
    else
    {
        test_if_else_x = test_if_else_x + 7;
    }
}
