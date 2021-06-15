#include <stdio.h>
#include <math.h> 

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main(){
  start_slice();
  float x = 1.0;
  float y = 0.9999999;
  float z = x - y;
  printf("z:%e", z);
  end_slice();
}
