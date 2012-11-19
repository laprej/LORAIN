int test_array_i;
int test_array_x[10];

void undo_test_array(void);

void test_array(void)
{
    for (test_array_i = 0; test_array_i < 10; test_array_i++) {
        test_array_x[test_array_i] = test_array_i + 1;
    }
}
