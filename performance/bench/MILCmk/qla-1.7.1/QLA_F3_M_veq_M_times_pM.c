/**************** QLA_F3_M_veq_M_times_pM.c ********************/

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


void QLA_F3_M_veq_M_times_pM ( QLA_F3_ColorMatrix *restrict r, QLA_F3_ColorMatrix *restrict a, QLA_F3_ColorMatrix *restrict *b, int n)
{
  start_slice();
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
      for(int j_c=0; j_c<3; j_c++) {
        QLA_F_Complex x;
        QLA_c_eq_r(x,0.);
        for(int k_c=0; k_c<3; k_c++) {
          QLA_c_peq_c_times_c(x, QLA_F3_elem_M(a[i],i_c,k_c), QLA_F3_elem_M(*b[i],k_c,j_c));
        }
        QLA_c_eq_c(QLA_F3_elem_M(r[i],i_c,j_c),x);
      }
    }
  }
  end_slice();
}
