/******** mat_invert.c *************/
/* MIMD version 6*/
/* DT 6/97
 * separated from spectrum_mom.c 12/97
 * Modify check_invert() to compute magnitude of error 11/98 DT
 */
/* Kogut-Susskind fermions  -- this version for "fat plus Naik"
   or general "even plus odd" quark actions.  Assumes "dslash" has
   been defined to be the appropriate "dslash_fn" or "dslash_eo"
 */
#include "generic_ks_includes.h"
#ifdef FN
#define dslash dslash_fn
#endif
#ifdef EO
#define dslash dslash_eo
#endif

/* Compute M^-1 * phi, answer in dest
   Uses phi, ttt, resid, xxx, and cg_p as workspace */
int mat_invert_cg( field_offset src, field_offset dest, field_offset temp,
    double mass ){
  int cgn;
  double finalrsq;
  clear_latvec( dest, EVENANDODD );
  cgn = ks_congrad( src, dest, mass,
      niter,rsqprop,EVENANDODD,&finalrsq);
  /* Multiply by Madjoint */
  dslash( dest, F_OFFSET(ttt), EVENANDODD);
  scalar_mult_add_latvec( F_OFFSET(ttt), dest,
      -2.0*mass, F_OFFSET(ttt), EVENANDODD);
  scalar_mult_latvec( F_OFFSET(ttt), -1.0, dest, EVENANDODD );
  return(cgn);
}


/* Invert using Leo's UML trick */

/* Our M is     (  2m		D_eo   )
   ( -D_eo^adj	2m )
   Note D_oe = -D_eo^adj 

   define U =	( 2m		-D_eo )
   (  0		2m    )

   L =   ( 2m		0  )
   ( D_eo^adj	2m )

   observe UML =    2m ( 4m^2+D_eo D_eo^adj	0    )
   (  0			4m^2 )
   and the upper left corner is what our conjugate gradient(even)
   inverts, except for a funny minus sign in our cong. grad. and a
   factor of 2m

   Use M^-1 phi = L L^-1 M^-1 U^-1 U phi
   = L  (UML)^-1 U phi
   = L  -(1/2m)(congrad) U phi
 */

int mat_invert_uml(field_offset src, field_offset dest, field_offset temp,
    double mass ){
  int cgn;
  double finalrsq;
  register int i;
  register site *s;

  if( src==temp ){
    printf("BOTCH\n"); exit(0);
  }
  /* multiply by U - even sites only */
  dslash( src, F_OFFSET(ttt), EVEN);
  scalar_mult_add_latvec( F_OFFSET(ttt), src,
      -2.0*mass, temp, EVEN);
  scalar_mult_latvec( temp, -1.0, temp, EVEN);
  /* invert with M_adj M even */
  cgn = ks_congrad( temp, dest, mass, niter, rsqprop,
      EVEN, &finalrsq );
  /* multiply by (1/2m)L, does nothing to even sites */
  /* fix up odd sites , 1/2m (Dslash_oe*dest_e + phi_odd) */
  dslash( dest, F_OFFSET(ttt), ODD );
  FORODDSITES(i,s){
    sub_su3_vector( (su3_vector *)F_PT(s,src), &(s->ttt), 
        (su3_vector *)F_PT(s,dest) );
    scalar_mult_su3_vector( (su3_vector *)F_PT(s,dest), 1.0/(2.0*mass),
        (su3_vector *)F_PT(s,dest) );
  }
  return(cgn);
}

/* FOR TESTING: multiply src by matrix and check against dest */
void check_invert( field_offset src, field_offset dest, double mass,
    double tol){
  register int i,k,flag;
  register site *s;
  double r_diff, i_diff;
  double sum,sum2;
  dslash( src, F_OFFSET(cg_p), EVENANDODD);
  FORALLSITES(i,s){
    scalar_mult_add_su3_vector( &(s->cg_p), (su3_vector *)F_PT(s,src),
        +2.0*mass, &(s->cg_p) );
  }
  sum2=sum=0.0;
  FORALLSITES(i,s){
    for(flag=0,k=0;k<3;k++){
      r_diff = ((su3_vector *)F_PT(s,dest))->c[k].real
        - s->cg_p.c[k].real;
      i_diff = ((su3_vector *)F_PT(s,dest))->c[k].imag
        - s->cg_p.c[k].imag;
      if( fabs(r_diff) > tol || fabs(i_diff) > tol )flag=1;
      if(flag)printf("%d %d  ( %.4e , %.4e )  ( %.4e , %.4e )\n",
          i,k,
          ((su3_vector *)F_PT(s,dest))->c[k].real,
          ((su3_vector *)F_PT(s,dest))->c[k].imag,
          s->cg_p.c[k].real,s->cg_p.c[k].imag);
      if(flag)terminate(0);
      sum += r_diff*r_diff + i_diff*i_diff;
    }
    sum2 += magsq_su3vec( (su3_vector *)F_PT(s,dest) );
  }
  g_doublesum( &sum );
  g_doublesum( &sum2 );
  g_sync(); if(this_node==0){
    printf("Inversion checked, frac. error = %e\n",sqrt(sum/sum2));
    fflush(stdout);
  }
}
