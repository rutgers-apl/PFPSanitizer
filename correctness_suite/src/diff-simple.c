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
  double x,y;
  x = 1e16;
  y = (x + 1) - x;
  printf("%e\n", y);
  end_slice();
  return 0;
}
