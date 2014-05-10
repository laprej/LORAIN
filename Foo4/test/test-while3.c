#include <stdio.h>

int test_while_x3;

int j;

void undo_test_while3(void);

void test_while3(void)
{
    j = 5;
    
    while (test_while_x3 < j) {
        test_while_x3++;
        
        if (test_while_x3 % 2 == 0) {
            printf("test_while3 is even\n");
        }
        else {
            printf("test_while3 is odd\n");
        }
    }
}
