#include <stdio.h>
#include <math.h>

// From the motivation example of Tao Bao's Oopsla13 "On the fly..." paper
// In float, zi = 0.
// In reals, zi = 1.

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main( int argc, const char* argv[] )
{
  start_slice();
  float x = 184.08964199999999777901393827051E+0;
  float z = x * x * x * x - 4 * x * x * x + 6 * x * x - 4 * x + 1;
  // Round to nearest integer
  z = z + 0.5;
  int zi = (int)(z + 0.5);
  printf("%d\n", zi);
  printf("%e\n", z);
  end_slice();
  return 0;
}


