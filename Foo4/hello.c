#include <stdio.h>

int bf;

int x;
//int y;
int abc;

void barbar(void);

void foobar(void)
{
  //  int i;
  //  int y = 42;
  //  y = y * 2;
  //  x = x * 3;
  //  x = x - y;
  ///*

  ///*
  if (x > 0) {
    x = x + 1;
  }
  //*/
  //  int y = 2;
  //  y = y * 3;
  //  y = y / 2;
  //  y = y * x;
  //  x = x * y;
  x = x * 3;
  x = x - 3;
  x = x * 4;
}

int main()
{
  bf = 0;

  x = 16;
  //  y = 42;

  printf("The value of x is %d\n", x);

  foobar();

  printf("The value of x is %d\n", x);

  barbar();

  printf("The value of x is %d\n", x);

  return 0;
}
