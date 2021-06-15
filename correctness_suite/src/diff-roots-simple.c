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
  x = 1e16;
  y = sqrt(x + 1) - sqrt(x);
  printf("%e\n", y);
  end_slice();
  return 0;
}
