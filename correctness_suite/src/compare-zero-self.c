#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  volatile double x;
  int cmp;
  x = 0.0;
  cmp = (x == 0.0);
  printf("%d\n", cmp);
  end_slice();
}
