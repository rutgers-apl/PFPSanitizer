#include <stdio.h>
#include <math.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

void calcY(double* y, double x){
  *y = sqrt(x + 1) - sqrt(x);
}

int main() {
  start_slice();
  double x,y;
  x = 1e10;
  calcY(&y, x);
  calcY(&y, x);
  printf("%e\n", y);
  end_slice();
  return 0;
}
