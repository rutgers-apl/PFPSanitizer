/**************** QLA_F3_r_veq_norm2_V.c ********************/

#include <stdio.h>
#include <qla_config.h>
#include <qla_types.h>
#include <qla_random.h>
#include <qla_cmath.h>
#include <qla_f3.h>
#include <math.h>

static void start_slice(){
  __asm__ __volatile__ ("");
}

static void end_slice(){
  __asm__ __volatile__ ("");
}

void QLA_F3_r_veq_norm2_V ( QLA_F_Real *restrict r, QLA_F3_ColorVector *restrict a, int n)
{
  start_slice();
#ifdef HAVE_XLC
#pragma disjoint(*r,*a)
  __alignx(16,r);
  __alignx(16,a);
#endif
  QLA_D_Real sum;
  sum = 0.;
#pragma omp parallel for reduction(+:sum)
  for(int i=0; i<n; i++) {
    for(int i_c=0; i_c<3; i_c++) {
      QLA_F_Complex at;
      QLA_c_eq_c(at,QLA_F3_elem_V(a[i],i_c));
      sum += QLA_norm2_c(at);
    }
  }
  *r = sum;
  end_slice();
}
