/**************** QLA_F3_V_veq_Ma_times_V.c ********************/

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

void QLA_F3_V_veq_Ma_times_V ( QLA_F3_ColorVector *restrict r, QLA_F3_ColorMatrix *restrict a, QLA_F3_ColorVector *restrict b, int n)
{
  start_slice();
#ifdef HAVE_XLC
#pragma disjoint(*r,*a,*b)
  __alignx(16,r);
  __alignx(16,a);
  __alignx(16,b);
#endif
#pragma omp parallel for
  for(int i=0; i<n; i++) {
    for(int i_c=0; i_c<3; i_c++) {
      QLA_F_Complex x;
      QLA_c_eq_r(x,0.);
      for(int k_c=0; k_c<3; k_c++) {
        QLA_c_peq_ca_times_c(x, QLA_F3_elem_M(a[i],k_c,i_c), QLA_F3_elem_V(b[i],k_c));
      }
      QLA_c_eq_c(QLA_F3_elem_V(r[i],i_c),x);
    }
  }
  end_slice();
}
