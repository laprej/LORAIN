int test_if_x3_1;
int test_if_x3_2;
int test_if_x_condition3_1;
int test_if_x_condition3_2;

void undo_test_if3(void);

void test_if3(void)
{
    if (test_if_x_condition3_1) {
        test_if_x3_1 = test_if_x3_1 + 1;
        
        if (test_if_x_condition3_2) {
            test_if_x3_2 = test_if_x3_2 - 3;
        }
    }
}
