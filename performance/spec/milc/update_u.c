/*************** update_u.c ************************************/
/* MIMD version 6 */

/* update the link matrices					*
 *  								*
 *  Go to sixth order in the exponential of the momentum		*
 *  matrices, since unitarity seems important.			*
 *  Evaluation is done as:					*
 *	exp(H) * U = ( 1 + H + H^2/2 + H^3/3 ...)*U		*
 *	= U + H*( U + (H/2)*( U + (H/3)*( ... )))		*
 *								*
 */

#include "ks_imp_includes.h"	/* definitions files and prototypes */

static void test_slice(){
  __asm__ __volatile__ ("");
}
static void start_slice(){
  __asm__ __volatile__ ("");
}
static void end_slice(){
  __asm__ __volatile__ ("");
}

void update_u_slice( double eps, int start, int end){
  start_slice();
  register int i,dir;
  register site *s;
  su3_matrix *link,temp1,temp2,htemp;
  register double t2,t3,t4,t5,t6;
  /**TEMP**
    double gf_x,gf_av,gf_max;
    int gf_i,gf_j;
   **END TEMP **/

  /**double dtime,dtime2,dclock();**/
  /**dtime = -dclock();**/

  /* Temporary by-hand optimization until pgcc compiler bug is fixed */
  t2 = eps/2.0;
  t3 = eps/3.0;
  t4 = eps/4.0;
  t5 = eps/5.0;
  t6 = eps/6.0;

  /** TEMP **
    gf_av=gf_max=0.0;
   **END TEMP**/
  for(i=start,s=lattice;i<end;i++,s++){
//  FORALLSITES(i,s){
    for(dir=XUP; dir <=TUP; dir++){
      uncompress_anti_hermitian( &(s->mom[dir]) , &htemp );
      link = &(s->link[dir]);
      mult_su3_nn(&htemp,link,&temp1);
      /**scalar_mult_add_su3_matrix(link,&temp1,eps/6.0,&temp2);**/
      scalar_mult_add_su3_matrix(link,&temp1,t6,&temp2);
      mult_su3_nn(&htemp,&temp2,&temp1);
      /**scalar_mult_add_su3_matrix(link,&temp1,eps/5.0,&temp2);**/
      scalar_mult_add_su3_matrix(link,&temp1,t5,&temp2);
      mult_su3_nn(&htemp,&temp2,&temp1);
      /**scalar_mult_add_su3_matrix(link,&temp1,eps/4.0,&temp2);**/
      scalar_mult_add_su3_matrix(link,&temp1,t4,&temp2);
      mult_su3_nn(&htemp,&temp2,&temp1);
      /**scalar_mult_add_su3_matrix(link,&temp1,eps/3.0,&temp2);**/
      scalar_mult_add_su3_matrix(link,&temp1,t3,&temp2);
      mult_su3_nn(&htemp,&temp2,&temp1);
      /**scalar_mult_add_su3_matrix(link,&temp1,eps/2.0,&temp2);**/
      scalar_mult_add_su3_matrix(link,&temp1,t2,&temp2);
      mult_su3_nn(&htemp,&temp2,&temp1);
      scalar_mult_add_su3_matrix(link,&temp1,eps    ,&temp2); 
      su3mat_copy(&temp2,link);
    }
  }
#ifdef FN
  valid_longlinks=0;
  valid_fatlinks=0;
#endif
  end_slice();
  /**dtime += dclock();
    node0_printf("LINK_UPDATE: time = %e  mflops = %e\n",
    dtime, (double)(5616.0*volume/(1.0e6*dtime*numnodes())) );**/
} /* update_u */

void update_u( double eps ){
  update_u_slice(eps, 0, 500);
  update_u_slice(eps, 500, 1000);
  update_u_slice(eps, 1000, 1500);
  update_u_slice(eps, 1500, 2000);
} /* update_u */
