#include <stdio.h>
void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

void compute(){
  start_slice();
  double u, v, w; int i, max;
  max = 31;
  u = 2;
  v = -4;
  printf("Computation from 3 to n:\n"); 
  for (i = 3; i <= max; i++)
  {
    w = 111. - 1130./v + 3000./(v*u); 
    u = v;
    v = w;
  } 
  end_slice();
}

int main(void) {
  compute();
  return 0;
}
