int test_if_x2_1;
int test_if_x2_2;
int test_if_x_condition2_1;
int test_if_x_condition2_2;

void undo_test_if2(void);

void test_if2(void)
{
    if (test_if_x_condition2_1) {
        test_if_x2_1 = test_if_x2_1 + 1;
    }
    
    if (test_if_x_condition2_2) {
        test_if_x2_2 = test_if_x2_2 - 3;
    }
}
