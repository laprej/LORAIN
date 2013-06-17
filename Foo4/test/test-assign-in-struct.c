typedef struct {
  int x;
  int y;
} foo;

foo test_assign_x;

void undo_test_assign(void);

void test_assign(void)
{
    test_assign_x.x = 3;
}
