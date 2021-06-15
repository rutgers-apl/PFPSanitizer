#include <stdio.h>
#include <math.h>

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

double inputs[12] = {
  0.7745966692414830, 0.7745966692414831, 0.7745966692414832, 0.7745966692414833,
  0.7745966692414834, 0.7745966692414835, 0.7745966692414836, 0.7745966692414837,
  0.7745966692414838, 0.7745966692414840, 0.7745966692414841, 0.7745966692414842
};

int main() {
  start_slice();
  volatile double x, y;
  for (int i = 0; i < 12; i++) {
    x = inputs[i];
    y = 5.0*x*x - 3.0;
    printf("%e\n", y);
  }
  end_slice();
  return 0;
  
}

