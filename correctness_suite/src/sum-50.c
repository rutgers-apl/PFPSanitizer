#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  volatile double x = 0;
  while(x < 10.0){
    x += 0.2;
  }
  printf("%.20g\n", x);
  end_slice();
}
