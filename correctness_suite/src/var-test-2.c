#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

double addTwo(double x, double y){
  return x + y;
}

int main() {
  start_slice();
  volatile double x, y, z;
  z = 3;
  x = addTwo(z + 2, 4);
  y = addTwo(5, 7);
  printf("%e\n%e\n", x, y);
  end_slice();
}
