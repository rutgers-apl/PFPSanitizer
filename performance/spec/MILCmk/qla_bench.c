#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <qla.h>
#ifdef _OPENMP
# include <omp.h>
# define __USE_GNU
# include <sched.h>
#endif

#if QLA_Precision == 'F'
#define REALBYTES 4
#else
#define REALBYTES 8
#endif
#define NC QLA_Nc

#define myalloc(type, n) (type *) aligned_malloc(n*sizeof(type))

#define ALIGN 64

static void start_slice(){
  __asm__ __volatile__ ("");
}

static void end_slice(){
  __asm__ __volatile__ ("");
}

void *
aligned_malloc(size_t n)
{
  size_t m = (size_t) malloc(n+ALIGN);
  size_t r = m % ALIGN;
  if(r) m += (ALIGN - r);
  return (void *)m;
}

double
dtime(void)
{
#ifdef _OPENMP
  return omp_get_wtime();
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return CLOCKS_PER_SEC*(tv.tv_sec + 1e-6*tv.tv_usec);
#endif
}

void
set_R(QLA_Real *r, int i)
{
  *r = 1+cos(i);
}

void
set_C(QLA_Complex *c, int i)
{
  QLA_c_eq_r_plus_ir(*c, 1+cos(i), 1+sin(i));
}

void
set_V(QLA_ColorVector *v, int i)
{
  for(int j=0; j<QLA_Nc; j++) {
    QLA_c_eq_r_plus_ir(QLA_elem_V(*v,j), j+1+cos(i), j+1+sin(i));
    //QLA_real(QLA_elem_V(*v,j)) = 1;
    //QLA_imag(QLA_elem_V(*v,j)) = 0;
  }
}

void
set_H(QLA_HalfFermion *h, int i)
{
  for(int j=0; j<QLA_Nc; j++) {
    for(int k=0; k<(QLA_Ns/2); k++) {
      QLA_c_eq_r_plus_ir(QLA_elem_H(*h,j,k), (j+4)*(k+1)+cos(i), (j+4)*(k+1)+sin(i));
    }
  }
}

void
set_D(QLA_DiracFermion *d, int i)
{
  for(int j=0; j<QLA_Nc; j++) {
    for(int k=0; k<QLA_Ns; k++) {
      QLA_c_eq_r_plus_ir(QLA_elem_D(*d,j,k), (j+4)*(k+1)+cos(i), (j+4)*(k+1)+sin(i));
    }
  }
}

void
set_M(QLA_ColorMatrix *m, int i)
{
  for(int j=0; j<QLA_Nc; j++) {
    for(int k=0; k<QLA_Nc; k++) {
      QLA_c_eq_r_plus_ir(QLA_elem_M(*m,j,k),
			 (((j-k+QLA_Nc+1)*(j+k+1))%19)+cos(i),
			 (((j+4)*(k+1))%17)+sin(i));
    }
  }
}

QLA_Real
sum_C(QLA_Complex *d, int n)
{
  QLA_Real t=0, *r=(QLA_Real *)d;
  int nn = n*sizeof(QLA_Complex)/sizeof(QLA_Real);
  for(int i=0; i<nn; i++) t += r[i];
  return t/nn;
}

QLA_Real
sum_V(QLA_ColorVector *d, int n)
{
  QLA_Real t=0, *r=(QLA_Real *)d;
  int nn = n*sizeof(QLA_ColorVector)/sizeof(QLA_Real);
  for(int i=0; i<nn; i++) t += r[i];
  return t/nn;
}

QLA_Real
sum_H(QLA_HalfFermion *d, int n)
{
  QLA_Real t=0, *r=(QLA_Real *)d;
  int nn = n*sizeof(QLA_HalfFermion)/sizeof(QLA_Real);
  for(int i=0; i<nn; i++) t += r[i];
  return t/nn;
}

QLA_Real
sum_D(QLA_DiracFermion *d, int n)
{
  QLA_Real t=0, *r=(QLA_Real *)d;
  int nn = n*sizeof(QLA_DiracFermion)/sizeof(QLA_Real);
  for(int i=0; i<nn; i++) t += r[i];
  return t/nn;
}

QLA_Real
sum_M(QLA_ColorMatrix *d, int n)
{
  QLA_Real t=0, *r=(QLA_Real *)d;
  int nn = n*sizeof(QLA_ColorMatrix)/sizeof(QLA_Real);
  for(int i=0; i<nn; i++) t += r[i];
  return t/nn;
}

#define set_fields { \
  _Pragma("omp parallel for") \
  for(int i=0; i<n; ++i) { \
    set_R(&r1[i], i); \
    set_C(&c1[i], i); \
    set_V(&v1[i], i); \
    set_V(&v2[i], i); \
    set_D(&d1[i], i); \
    set_D(&d2[i], i); \
    set_M(&m1[i], i); \
    set_M(&m2[i], i); \
    set_M(&m3[i], i); \
    int j = ((i|16)+256) % n; \
    vp1[i] = &v2[j]; \
    dp1[i] = &d2[j]; \
    mp1[i] = &m3[j]; \
  } \
}

void QLA_D3_V_vpeq_M_times_pV ( QLA_D3_ColorVector *restrict r, QLA_D3_ColorMatrix *restrict a, QLA_D3_ColorVector **b, int n)
{
//  start_slice();
#ifdef HAVE_XLC
#pragma disjoint(*r,*a,**b)
  __alignx(16,r);
  __alignx(16,a);
#endif
#pragma omp parallel for
  for(int i=0; i<n; i++) {
#ifdef HAVE_XLC
    __alignx(16,b[i]);
#endif
    for(int i_c=0; i_c<3; i_c++) {
      QLA_D_Complex x;
      QLA_c_eq_c(x,QLA_D3_elem_V(r[i],i_c));
      for(int k_c=0; k_c<3; k_c++) {
        QLA_c_peq_c_times_c(x, QLA_D3_elem_M(a[i],i_c,k_c), QLA_D3_elem_V(*b[i],k_c));
      }
      QLA_c_eq_c(QLA_D3_elem_V(r[i],i_c),x);
    }
  }
//  end_slice();
}

void slice11(QLA_ColorVector *v1, QLA_ColorMatrix *m1, 
    QLA_ColorVector **vp1, int n, int c, int start, int end){
  start_slice();
  for(int i=start; i<end; ++i) {
    QLA_V_vpeq_M_times_pV(v1, m1, vp1, n);
  }
  end_slice();
}

void slice1(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;
    set_fields;
    mem = 2*(3+NC)*NC*REALBYTES;
    flop = 8*NC*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    int start = 0;
    for(int i=0; i<c; i=i+1000){
      slice11(v1, m1, vp1, n, c, 0, 1000);
      start = i;
    }
    slice11(v1, m1, vp1, n, c, start, c);
    time1 = dtime() - time1;
    sum = sum_V(v1, n);
    printf("%-32s:", "QLA_V_vpeq_M_times_pV");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice2(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;

    set_fields;
    mem = 2*(2+NC)*NC*REALBYTES;
    flop = (8*NC-2)*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_V_veq_Ma_times_V(v1, m1, v2, n);
    }
    time1 = dtime() - time1;
    sum = sum_V(v1, n);
    printf("%-32s:", "QLA_V_veq_Ma_times_V");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice3(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;
    set_fields;
    mem = 6*NC*REALBYTES;
    flop = 2*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_V_vmeq_pV(v1, vp1, n);
    }
    time1 = dtime() - time1;
    sum = sum_V(v1, n);
    printf("%-32s:", "QLA_V_vmeq_pV");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice4(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;

    set_fields;
    mem = 2*(12+NC)*NC*REALBYTES;
    flop = (16*NC+8)*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_D_vpeq_spproj_M_times_pD(d1, m1, dp1,0,1,n);
    }
    time1 = dtime() - time1;
    sum = sum_D(d1,n);
    printf("%-32s:", "QLA_D_vpeq_spproj_M_times_pD");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice5(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;
    set_fields;
    mem = 6*NC*NC*REALBYTES;
    flop = (8*NC-2)*NC*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_M_veq_M_times_pM(m1, m2, mp1, n);
    }
    time1 = dtime() - time1;
    sum = sum_M(m1, n);
    printf("%-32s:", "QLA_M_veq_M_times_pM");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice6(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;
    set_fields;
    mem = 2*NC*REALBYTES;
    flop = 4*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_r_veq_norm2_V(r1, v1, n);
    }
    time1 = dtime() - time1;
    sum = *r1;
    printf("%-32s:", "QLA_r_veq_norm2_V");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void slice7(
    int n, QLA_Real* r1, QLA_Complex* c1,
    QLA_ColorVector *v1, QLA_ColorVector* v2,
    QLA_ColorVector **vp1, QLA_DiracFermion *d1,
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1,
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2,
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    double cf = 9.e9/n;
    double flop, mem, time1;
    int nmin, nmax, c, nthreads=1;
    set_fields;
    mem = 4*NC*REALBYTES;
    flop = 8*NC;
    c = 1 + cf/(flop+mem);
    time1 = dtime();
    for(int i=0; i<c; ++i) {
      QLA_c_veq_V_dot_V(c1, v1, v2, n);
    }
    time1 = dtime() - time1;
    sum = QLA_norm2_c(*c1);
    printf("%-32s:", "QLA_c_veq_V_dot_V");
    printf("%12g time=%5.2f mem=%5.0f mflops=%5.0f\n",          sum, time1, mem*n*c/(1e6*time1), flop*n*c/(1e6*time1));
}

void qla(int n, QLA_Real* r1, QLA_Complex* c1, 
    QLA_ColorVector *v1, QLA_ColorVector* v2, 
    QLA_ColorVector **vp1, QLA_DiracFermion *d1, 
    QLA_DiracFermion *d2, QLA_DiracFermion **dp1, 
    QLA_ColorMatrix *m1, QLA_ColorMatrix *m2, 
    QLA_ColorMatrix *m3, QLA_ColorMatrix **mp1){
    QLA_Real sum;
    int nmin, nmax, c, nthreads=1;
    double flop, mem, time1;
    printf("len = %i\n", n);
    printf("len/thread = %i\n", n/nthreads);
    double cf = 9.e9/n;

    slice1(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    /*
    slice2(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    slice3(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    slice4(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    slice5(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    slice6(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    slice7(n, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
    */
}

int
main(int argc, char *argv[])
{
  QLA_Real sum, *r1;
  QLA_Complex *c1;
  QLA_ColorVector *v1, *v2, *v3, *v4, *v5;
  QLA_ColorVector **vp1, **vp2, **vp3, **vp4;
  QLA_HalfFermion *h1, *h2, **hp1;
  QLA_DiracFermion *d1, *d2, **dp1;
  QLA_ColorMatrix *m1, *m2, *m3, *m4, **mp1;
  double flop, mem, time1;
  int nmin, nmax, c, nthreads=1;

  printf("QLA_Precision = %c\n", QLA_Precision);
#ifdef _OPENMP
  nthreads = omp_get_max_threads();
  printf("OMP THREADS = %i\n", nthreads);
  printf("omp_get_wtick = %g\n", omp_get_wtick());
#ifdef CPU_ZERO
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(tid, &set);
    sched_setaffinity(0, sizeof(set), &set);
  }
#endif
#endif

  nmin = 64*nthreads;
  nmax = 256*1024*nthreads;

  r1 = myalloc(QLA_Real, nmax);
  c1 = myalloc(QLA_Complex, nmax);
  v1 = myalloc(QLA_ColorVector, nmax);
  v2 = myalloc(QLA_ColorVector, nmax);
  vp1 = myalloc(QLA_ColorVector *, nmax);
  d1 = myalloc(QLA_DiracFermion, nmax);
  d2 = myalloc(QLA_DiracFermion, nmax);
  dp1 = myalloc(QLA_DiracFermion *, nmax);
  m1 = myalloc(QLA_ColorMatrix, nmax);
  m2 = myalloc(QLA_ColorMatrix, nmax);
  m3 = myalloc(QLA_ColorMatrix, nmax);
  mp1 = myalloc(QLA_ColorMatrix *, nmax);

  for(int n=nmin; n<=nmax; n*=2) {
    qla(nmin, r1, c1, v1, v2, vp1, d1, d2, dp1, m1, m2, m3, mp1);
  }

  return 0;
}
