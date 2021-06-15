#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  volatile double x = 1.0;
  double y = x + x;
  printf("%e\n", y);
  end_slice();
  return 0;
}
