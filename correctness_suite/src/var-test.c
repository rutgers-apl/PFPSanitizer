#include <stdio.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

double add(double x, double y){
  return x + y;
}

int main() {
  start_slice();
  volatile double x, y, z1, z2, z3;
  z1 = 5;
  z2 = 6;
  z3 = z1 + z2;
  x = add(4, 5);
  y = add(6, z3);
  end_slice();
}
