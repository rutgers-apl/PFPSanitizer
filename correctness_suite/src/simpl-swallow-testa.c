#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

double foo(double x){
  return ((x * 5) + 1) - (x * 5);
}

double bar(double x, double y){
  return ((x * 5) + (1 + y)) - ((x * 5) + y);
}

int main(int argc, char** argv){
  start_slice();
  volatile double x, y, z, t;
  x = 1.0;
  y = 1.0e16;
  z = foo(x);
  printf("z = %e\n", z);
  end_slice();
  return 0;
}
