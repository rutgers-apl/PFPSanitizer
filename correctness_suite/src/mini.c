#include <stdio.h>
#include <math.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  volatile double x,y;
  x = 3;
  y = sqrt(x + 1);
  printf("%e\n", y);
  end_slice();
  return 0;
}
