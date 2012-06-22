int test_while_x;

void undo_test_while(void);

void test_while(void)
{
    while (test_while_x < 5) {
        test_while_x++;
    }
}