#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  int x;
  x = 1 + 2;
  printf("%d\n", x);
  end_slice();
  return 0;
}
