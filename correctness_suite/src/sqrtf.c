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
  volatile float x, y;
  x = 4.0f;
  y = sqrtf(x);
  printf("%e\n", y);
  end_slice();
}
