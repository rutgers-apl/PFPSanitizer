/**************** QLA_F3_V_vmeq_pV.c ********************/

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

void QLA_F3_V_vmeq_pV ( QLA_F3_ColorVector *restrict r, QLA_F3_ColorVector *restrict *a, int n)
{
  start_slice();
#ifdef HAVE_XLC
#pragma disjoint(*r,**a)
  __alignx(16,r);
#endif
#pragma omp parallel for
  for(int i=0; i<n; i++) {
#ifdef HAVE_XLC
    __alignx(16,a[i]);
#endif
    for(int i_c=0; i_c<3; i_c++) {
      QLA_c_meq_c(QLA_F3_elem_V(r[i],i_c),QLA_F3_elem_V(*a[i],i_c));
    }
  }
  end_slice();
}
