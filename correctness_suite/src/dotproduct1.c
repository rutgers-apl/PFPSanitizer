#include <stdio.h>

// calculating dot product. The intermediate result has less precision bits in posit than float.
// Altered from End of error example.

void start_slice(){
  __asm__ __volatile__ ("");
}

void end_slice(){
  __asm__ __volatile__ ("");
}

int main( int argc, const char* argv[] )
{
  start_slice();
  float fa[4] = {1.000010223508783104E+18, 1.00002781569482752E+17, -1.000010223508783104E+18, -1.00002781569482752E+17};
  float fb[4] = {1.000010223508783104E+18, 1.00002781569482752E+17, 1.000010223508783104E+18, -1.00002781569482752E+17};
  float fresult = 0.0;
  for (int i = 0; i < 4; i++) {
    fresult += fa[i] * fb[i];
  }

  printf("float    result: %.50e\n", fresult);
  end_slice();
  return 0;
}
