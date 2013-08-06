int test_switch_x;

void undo_test_switch(void);

void test_switch(void)
{
    switch (test_switch_x) {
        case 0:
            test_switch_x += 37;
            break;
            
        case 42:
            test_switch_x += 14;
            break;
            
        case 790:
            test_switch_x += 134;
            break;
            
        default:
            test_switch_x += 1337;
            break;
    }
}
