#include <stdio.h>

int x;
//int y;
int abc;

void barbar(void);

void foobar(void)
{
  if (x > 0) {
    x = x + 1;
  }
  x = x + 14;
  x = x * 3;
  x = x - 3;
  x = x * 4;
}

int main()
{
  printf("Please enter an 0 or 1 for x: ");
  scanf("%d", &x);

  //x = 0;
  //  y = 42;

  printf("The value of x is %d\n", x);

  foobar();

  printf("The value of x is %d\n", x);

  barbar();

  printf("The value of x is %d\n", x);

  return 0;
}
