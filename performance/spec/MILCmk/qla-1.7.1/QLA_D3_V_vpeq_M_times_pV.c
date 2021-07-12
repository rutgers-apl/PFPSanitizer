/**************** QLA_D3_V_vpeq_M_times_pV.c ********************/

#include <stdio.h>
#include <qla_config.h>
#include <qla_types.h>
#include <qla_random.h>
#include <qla_cmath.h>
#include <qla_d3.h>
#include <math.h>

static void start_slice(){
  __asm__ __volatile__ ("");
}

static void end_slice(){
  __asm__ __volatile__ ("");
}
/*
void QLA_D3_V_vpeq_M_times_pV ( QLA_D3_ColorVector *restrict r, QLA_D3_ColorMatrix *restrict a, QLA_D3_ColorVector *restrict *b, int n)
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
*/
