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
  double x, y;
  x = atan(1.0) * (40002);
  y = tan(x) - (sin(x)/cos(x));
  printf("%e\n", y);
  end_slice();
  return 0;
}
