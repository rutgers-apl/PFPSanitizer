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
  volatile float x = 1e-20;
  volatile float y = x * (-1);
  volatile float z = 5.0;
  float w = (y + z) - z;
  printf("%e\n", w);
  end_slice();
}
