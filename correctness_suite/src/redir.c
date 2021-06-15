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
  volatile double x = 0.0;
  for (int i = 0; i < 2; ++i) {
    x += sqrt(i);
  }
  printf("%e\n", x);
  end_slice();
}
