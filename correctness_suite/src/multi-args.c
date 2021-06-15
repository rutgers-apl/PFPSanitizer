#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main() {
  start_slice();
  double xs[2] = {1.2, 1e-12};
  double ys[2] = {0.5, 1e12};
  double x, y, z;
  for(int i = 0; i < 2; ++i){
    x = xs[i];
    y = ys[i];
    z = ( x + y ) - y;
    printf("%f\n", z);
  }
  end_slice();
}
