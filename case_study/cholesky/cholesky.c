/**
 * This version is stamped on May 10, 2016
 *
 * Contact:
 *   Louis-Noel Pouchet <pouchet.ohio-state.edu>
 *   Tomofumi Yuki <tomofumi.yuki.fr>
 *
 * Web address: http://polybench.sourceforge.net
 */
/* cholesky.c: this file is part of PolyBench/C */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

/* Include polybench common header. */
#include <polybench.h>

/* Include benchmark-specific header. */
#include "cholesky.h"

void start_slice(){
    __asm__ __volatile__ ("");
}

void end_slice(){
    __asm__ __volatile__ ("");
}

double rand_double(int n) {
  union {
    double d;
    unsigned char uc[sizeof(double)];
  } u;
  do {
    for (size_t i = 0; i < sizeof(double); i++) {
      u.uc[i] = rand();
    }
  } while (isnan(u.d));
  //return fmod(u.d,1e+9);
  return fmod(u.d,1e+4);
}

/* Array initialization. */
static
void init_array(int n,
		DATA_TYPE POLYBENCH_2D(A,N,N,n,n))
{
  int i, j;

  for (i = 0; i < n; i++)
  {
    for (j = 0; j <= i; j++)
      A[i][j] = rand_double(n);
//      A[i][j] = (DATA_TYPE)(-j % n) / n + 1;
    for (j = i+1; j < n; j++) {
      A[i][j] = 0;
    }
    A[i][i] = 1;
  }

  printf("start A\n");
  for (i = 0; i < n; i++)
  {
    for (j = 0; j < n; j++){
      printf("%e  ", A[i][j]);
    }
    printf("\n");
  }

  /* Make the matrix positive semi-definite. */
  int r,s,t;
  POLYBENCH_2D_ARRAY_DECL(B, DATA_TYPE, N, N, n, n);
  for (r = 0; r < n; ++r)
    for (s = 0; s < n; ++s)
      (POLYBENCH_ARRAY(B))[r][s] = 0;
  for (t = 0; t < n; ++t)
    for (r = 0; r < n; ++r)
      for (s = 0; s < n; ++s)
        (POLYBENCH_ARRAY(B))[r][s] += A[r][t] * A[s][t];
  //for (r = 0; r < n; ++r)
    //for (s = 0; s < n; ++s)
      //A[r][s] = (POLYBENCH_ARRAY(B))[r][s];
  memcpy(A, B, sizeof(A)*n);
  POLYBENCH_FREE_ARRAY(B);

  printf("semi-definate\n");
  for (i = 0; i < n; i++)
  {
    for (j = 0; j < n; j++){
      printf("%e  ", A[i][j]);
    }
    printf("\n");
  }
}


/* DCE code. Must scan the entire live-out data.
   Can be used also to check the correctness of the output. */
static
void print_array(int n,
		 DATA_TYPE POLYBENCH_2D(A,N,N,n,n))

{
  int i, j;

  POLYBENCH_DUMP_START;
  POLYBENCH_DUMP_BEGIN("A");
  for (i = 0; i < n; i++)
    for (j = 0; j <= i; j++) {
    if ((i * n + j) % 20 == 0) fprintf (POLYBENCH_DUMP_TARGET, "\n");
    fprintf (POLYBENCH_DUMP_TARGET, DATA_PRINTF_MODIFIER, A[i][j]);
  }
  POLYBENCH_DUMP_END("A");
  POLYBENCH_DUMP_FINISH;
}


/* Main computational kernel. The whole function will be timed,
   including the call and return. */
static
void kernel_cholesky(int n,
		     DATA_TYPE POLYBENCH_2D(A,N,N,n,n))
{
  int i, j, k;


#pragma scop
  for (i = 0; i < _PB_N; i++) {
     //j<i
     for (j = 0; j < i; j++) {
        for (k = 0; k < j; k++) {
           A[i][j] -= A[i][k] * A[j][k];
        }
        A[i][j] /= A[j][j];
     }
     // i==j case
     for (k = 0; k < i; k++) {
        A[i][i] -= A[i][k] * A[i][k];
     }
     A[i][i] = SQRT_FUN(A[i][i]);
  }
#pragma endscop

}
int foo(int n){
  float x = n;
  x = x * x;
  return (int) x;
}

int cholesky()
{
  start_slice();
  /* Retrieve problem size. */
  int n = N;

  /* Variable declaration/allocation. */
  POLYBENCH_2D_ARRAY_DECL(A, DATA_TYPE, N, N, n, n);

  /* Initialize array(s). */
  init_array (n, POLYBENCH_ARRAY(A));
  int d = foo(n);
  printf("d:%d", d);
  /* Start timer. */
  polybench_start_instruments;

  /* Run kernel. */
  kernel_cholesky (n, POLYBENCH_ARRAY(A));

  /* Stop and print timer. */
  polybench_stop_instruments;
  polybench_print_instruments;

  /* Prevent dead-code elimination. All live-out data must be printed
     by the function call in argument. */
  print_array(n, POLYBENCH_ARRAY(A));

  /* Be clean. */
  POLYBENCH_FREE_ARRAY(A);
  end_slice();
  return 0;
}

int main(){
  cholesky();
}
