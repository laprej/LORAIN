int test_while_x2;

int i;

void undo_test_while2(void);

void test_while2(void)
{
    i = 5;
    
    while (test_while_x2 < i) {
        test_while_x2++;
    }
}
