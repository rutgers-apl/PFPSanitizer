/****************  m_mv_s_4dir.c  (in su3.a) *****************************
*									*
* void mult_su3_mat_vec_sum_4dir( su3_matrix *a, su3_vector *b[0123],*c )*
* Multiply the elements of an array of four su3_matrices by the		*
* four su3_vectors, and add the results to				*
* produce a single su3_vector.						*
* C  <-  A[0]*B[0]+A[1]*B[1]+A[2]*B[2]+A[3]*B[3]			*
*/
#include "config.h"
#include "complex.h"
#include "su3.h"
static void test_slice(){
  __asm__ __volatile__ ("");
}
static void start_slice(){
  __asm__ __volatile__ ("");
}
static void end_slice(){
  __asm__ __volatile__ ("");
}

#ifndef FAST
void mult_su3_mat_vec_sum_4dir(  su3_matrix *a, su3_vector *b0,
       su3_vector *b1, su3_vector *b2, su3_vector *b3, su3_vector *c  ){
    mult_su3_mat_vec( a+0,b0,c );
    mult_su3_mat_vec_sum( a+1,b1,c );
    mult_su3_mat_vec_sum( a+2,b2,c );
    mult_su3_mat_vec_sum( a+3,b3,c );
}

#else
/* Fast code, with subroutines inlined */
#ifdef NATIVEDOUBLE /* IBM RS6000 version */
void mult_su3_mat_vec_sum_4dir(  su3_matrix *a, su3_vector *b0,
       su3_vector *b1, su3_vector *b2, su3_vector *b3, su3_vector *c  ){
  register int n;
  register double c0r,c0i,c1r,c1i,c2r,c2i;
  register double br,bi,a0,a1,a2;
  register su3_matrix *mat;
  register su3_vector *b;

  c0r = c0i = c1r = c1i = c2r = c2i = 0.0;
  mat = a;

  for(n=0;n<4;n++,mat++){

  switch(n){
    case(0): b=b0; break;
    case(1): b=b1; break;
    case(2): b=b2; break;
    case(3): b=b3; break;
  }

  br=b->c[0].real;    bi=b->c[0].imag;
  a0=mat->e[0][0].real;
  a1=mat->e[1][0].real;
  a2=mat->e[2][0].real;

  c0r += a0*br;
  c1r += a1*br;
  c2r += a2*br;
  c0i += a0*bi;
  c1i += a1*bi;
  c2i += a2*bi;

  a0=mat->e[0][0].imag;
  a1=mat->e[1][0].imag;
  a2=mat->e[2][0].imag;

  c0r -= a0*bi;
  c1r -= a1*bi;
  c2r -= a2*bi;
  c0i += a0*br;
  c1i += a1*br;
  c2i += a2*br;

  br=b->c[1].real;    bi=b->c[1].imag;
  a0=mat->e[0][1].real;
  a1=mat->e[1][1].real;
  a2=mat->e[2][1].real;

  c0r += a0*br;
  c1r += a1*br;
  c2r += a2*br;
  c0i += a0*bi;
  c1i += a1*bi;
  c2i += a2*bi;

  a0=mat->e[0][1].imag;
  a1=mat->e[1][1].imag;
  a2=mat->e[2][1].imag;

  c0r -= a0*bi;
  c1r -= a1*bi;
  c2r -= a2*bi;
  c0i += a0*br;
  c1i += a1*br;
  c2i += a2*br;

  br=b->c[2].real;    bi=b->c[2].imag;
  a0=mat->e[0][2].real;
  a1=mat->e[1][2].real;
  a2=mat->e[2][2].real;

  c0r += a0*br;
  c1r += a1*br;
  c2r += a2*br;
  c0i += a0*bi;
  c1i += a1*bi;
  c2i += a2*bi;

  a0=mat->e[0][2].imag;
  a1=mat->e[1][2].imag;
  a2=mat->e[2][2].imag;

  c0r -= a0*bi;
  c1r -= a1*bi;
  c2r -= a2*bi;
  c0i += a0*br;
  c1i += a1*br;
  c2i += a2*br;

  }

  c->c[0].real = c0r;
  c->c[0].imag = c0i;
  c->c[1].real = c1r;
  c->c[1].imag = c1i;
  c->c[2].real = c2r;
  c->c[2].imag = c2i;
}

#else
void mult_su3_mat_vec_sum_4dir(  su3_matrix *a, su3_vector *b0,
       su3_vector *b1, su3_vector *b2, su3_vector *b3, su3_vector *c  ){
  int i,n;
  register su3_matrix *at;
  register su3_vector *b;
  register double t,ar,ai,br,bi,cr,ci;
  
  for(i=0;i<3;i++){
    c->c[i].real = 0.0;
    c->c[i].imag = 0.0;
  }
  for(n=0;n<4;n++){
    at = a+n;
    switch(n){
    case(0): b=b0; break;
    case(1): b=b1; break;
    case(2): b=b2; break;
    case(3): b=b3; break;
    }
    for(i=0;i<3;i++){
      
      ar=at->e[i][0].real; ai=at->e[i][0].imag;
      br=b->c[0].real; bi=b->c[0].imag;
      cr=ar*br; t=ai*bi; cr -= t;
      ci=ar*bi; t=ai*br; ci += t;
      
      ar=at->e[i][1].real; ai=at->e[i][1].imag;
      br=b->c[1].real; bi=b->c[1].imag;
      t=ar*br; cr += t; t=ai*bi; cr -= t;
      t=ar*bi; ci += t; t=ai*br; ci += t;
      
      ar=at->e[i][2].real; ai=at->e[i][2].imag;
      br=b->c[2].real; bi=b->c[2].imag;
      t=ar*br; cr += t; t=ai*bi; cr -= t;
      t=ar*bi; ci += t; t=ai*br; ci += t;
      
      c->c[i].real += cr;
      c->c[i].imag += ci;
    }
  }
}
#endif  /* End of "#ifdef NATIVEDOUBLE" */
#endif	/* End of "#ifdef FAST" */
