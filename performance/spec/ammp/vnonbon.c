/* vnonbon.c
*
* collection of routines to service nonbonded potentials
*
* POOP (Poor-mans Object Oriented Programming) using scope rules
*
* the routines for potential value, force and (eventually) second
* derivatives are here also
*
* force and 2nd derivative routines assume zero'd arrays for output
* this allows for parralellization if needed (on a PC?)
*
* forces are symmetric - so we don't have to mess around with
* s matrices and the like.
*
* note that the non-bonded information is in the ATOM structures 
*
*
* attempts at vectorization
*/
/*
*  copyright 1992, 1993 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/
static void start_slice(){
  __asm__ __volatile__ ("");
}

static void end_slice(){
  __asm__ __volatile__ ("");
}

/* SPEC add function proto to reduce compiler warnings jh/9/21/99 */
void a_inactive_f_zero ();
#define ANSI 1
/* misc includes - ANSI and some are just to be safe */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef ANSI
#include <stdlib.h>
#endif
#include "ammp.h"
int mm_fv_update_nonbon(  float lambda );
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-nonboned potentials
*/

/* v_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int fv_update_nonbon(  lambda )
	float lambda;
{
	void a_inactive_f_zero(void);
	float r,r0,xt,yt,zt;
	float k,k1,k2,k3,k4,k5;
	float ka3,ka2;
	float kb3,kb2;
	int inbond,inangle,i;
	ATOM *a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next( ); /* returns first ATOM when called with -1 */
	int (*indexes)[],inindex,in;
	int a_number();
	int ii,j,jj,imax,inclose;
	float (*vector)[];
/*	float (*vecold)[];
*/
	ATOM *close[NCLOSE],*(*atomall)[];
	float mxdq,dielectric,mxcut; 
	static float dielecold = -1.;


  float get_f_variable(char *name);
	mxdq = get_f_variable("mxdq");
/*	if( mxdq < 0.) mxdq = 0.;
*/
	mxcut = get_f_variable("mxcut");
	if( mxcut < 0.) mxcut= 5.;
	if( mxdq > 0.) mxdq = mxdq*mxdq;

	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	if( dielecold != dielectric)
	{
	 dielecold = dielectric;
	mxdq = -1.;
	}
	dielectric = 332.17752/dielectric;
	
/*  get the number of atoms and allocate the memory for the array space */
	i = a_number();
	vector = malloc( 4*i*sizeof(float) );
	if( vector == NULL) 
	{ aaerror("cannot allocate memory in v_nonbon\n"); return 0;}
/*
	vecold = malloc( 4*i*sizeof(float) );
	if( vecold == NULL) 
	{ aaerror("cannot allocate memory in v_nonbon\n"); return 0;}
*/
	atomall = malloc( i*sizeof(ATOM *) );
	if( atomall == NULL)
	{aaerror("cannot allocate memory in v_nonbon\n"); return 0;}

	imax = a_number();
	for( i=0; i< imax; i++)
	{
		(*atomall)[i] = a_next(i);
	}
/* first check if anyone's moved and update the lists */
/* note that this must be a look-ahead rather than
*  look back search because
* we cannot update ->px until we've used that atom !!! */
	for( ii=0; ii< imax; ii++)
	{
        a1 = (*atomall)[ii];
        xt = a1->dx*lambda +a1->x - a1->px;
        yt = a1->dy*lambda +a1->y - a1->py;
        zt = a1->dz*lambda +a1->z - a1->pz;
        r = xt*xt + yt*yt + zt*zt;
	if( r > mxdq ) goto DOIT;
	}
	
	free( vector);
/*	free( vecold);
*/
	free (atomall);
	return 1;
DOIT:
	xt = get_f_variable("mmbox");
	if( xt > 0.)
	{ free(vector); free(atomall); mm_fv_update_nonbon(lambda); return 1;}
	indexes = malloc( imax* sizeof(int) );
	if( indexes == NULL ){
	aaerror(" cannot allocate memory in fv_update\n");
	return 0;}
	for( ii=0; ii< imax; ii++)
	{
	a1 = (*atomall)[ii];
	a1 -> VP = 0.;
	a1 -> dpx = 0.;
	a1 -> dpy = 0.;
	a1 -> dpz = 0.;
	a1 -> qxx = 0.;
	a1 -> qxy = 0.;
	a1 -> qxz = 0.;
	a1 -> qyy = 0.;
	a1 -> qyz = 0.;
	a1 -> qzz = 0.;
#ifdef CUBIC
	a1 -> qxxx = 0.;
	a1 -> qxxy = 0.;
	a1 -> qxxz = 0.;
	a1 -> qxyy = 0.;
	a1 -> qxyz = 0.;
	a1 -> qxzz = 0.;
	a1 -> qyyy = 0.;
	a1 -> qyyz = 0.;
	a1 -> qyzz = 0.;
	a1 -> qzzz = 0.;
#endif
	for( j=0; j< NCLOSE; j++) 
		a1->close[j] = NULL;

	}
	for( ii=0; ii<  imax; ii++)
	{ /* if this is met we update the expansion for this atom */
	a1 = (*atomall)[ii];
	inclose = 0;
	if( lambda != 0.)
	{
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	j = i*4;
	(*vector)[j  ] = a2->x - a1->x + lambda*(a2->dx -a1->dx);
	(*vector)[j+1] = a2->y - a1->y + lambda*(a2->dy -a1->dy);
	(*vector)[j+2] = a2->z - a1->z + lambda*(a2->dz -a1->dz);
	}
	}else {
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	j = i*4;
	(*vector)[j  ] = a2->x - a1->x ;
	(*vector)[j+1] = a2->y - a1->y ;
	(*vector)[j+2] = a2->z - a1->z ;
	}
	} /* end of difference position into vector loops */
	for( i=ii+1; i< imax; i++)
	{
		j = i*4;
		(*vector)[j+3] = sqrt((*vector)[j]*(*vector)[j] +
				 (*vector)[j+1]*(*vector)[j+1] +
				 (*vector)[j+2]*(*vector)[j+2]);
	}
/* add the new components */
/* first extract indexes */
	inindex = 0;
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	for( j=0; j< a1->dontuse; j++)
	{ if( a2 == a1->excluded[j]) goto SKIPNEW;}
	j = i*4;
	if( (*vector)[j+3] > mxcut || inclose > NCLOSE)
	{
	(*indexes)[inindex++] = i;
	}else {
	a1->close[inclose++] = (*atomall)[i];
	}
	if( inclose == NCLOSE)
	{
	aaerror(
	" fv_update_nonbon> too many atoms increase NCLOSE or decrease mxcut");
	}
SKIPNEW:   i = i;
	}
	/* and then use them */
	for( in=0; in< inindex; in++)
	{
	i = (*indexes)[in];
	a2 = (*atomall)[i];
	j = i*4;
	r0 = one/(*vector)[j+3];  
	r = r0*r0;
	r = r*r*r; /* r0^-6 */
	xt = a1->q*a2->q*dielectric*r0;
	yt = a1->a*a2->a*r;
	zt = a1->b*a2->b*r*r;
	k = xt - yt + zt;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k1 = xt - yt*six + zt*twelve;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	/*
	k2 = xt*three; ka2 = - yt*6*8; kb2 =  zt*12*14;
	*/
	k2 = xt*three; ka2 = - yt*48.; kb2 =  zt*168.;
#ifdef CUBIC
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k3 = -xt*5*3; ka3 =   yt*6*8*10 ; kb3 =  -zt*12*14*16;
#endif
	k1 = -k1;
	xt = (*vector)[j]*r0 ;
	yt = (*vector)[j+1]*r0 ;
	zt = (*vector)[j+2] *r0;
	/*
	xt = (*vector)[j] ;
	yt = (*vector)[j+1] ;
	zt = (*vector)[j+2] ;
	*/
	a1->VP += k;
	a2->dpx -= k1*xt;
	a1->dpx += k1*xt;
	a2->dpy -= k1*yt;
	a1->dpy += k1*yt;
	a2->dpz -= k1*zt;
	a1->dpz += k1*zt;
/*  note that xt has the 1/r in it so k2*xt*xt is 1/r^5 */
	a2->qxx -= k2*(xt*xt - third) + ka2*(xt*xt - eightth) + kb2*(xt*xt-fourteenth) ;
	a1->qxx -= k2*(xt*xt - third) + ka2*(xt*xt - eightth) + kb2*(xt*xt-fourteenth) ;
	a2->qxy -= (k2+ka2+kb2)*yt*xt;
	a1->qxy -= (k2+ka2+kb2)*yt*xt;
	a2->qxz -= (k2+ka2+kb2)*zt*xt;
	a1->qxz -= (k2+ka2+kb2)*zt*xt;
	a2->qyy -= k2*(yt*yt - third) + ka2*(yt*yt - eightth) + kb2*(yt*yt-fourteenth) ;
	a1->qyy -= k2*(yt*yt - third) + ka2*(yt*yt - eightth) + kb2*(yt*yt-fourteenth) ;
	a2->qyz -= (k2+ka2+kb2)*yt*zt;
	a1->qyz -= (k2+ka2+kb2)*yt*zt;
	a2->qzz -= k2*(zt*zt - third) + ka2*(zt*zt - eightth) + kb2*(zt*zt-fourteenth) ;
	a1->qzz -= k2*(zt*zt - third) + ka2*(zt*zt - eightth) + kb2*(zt*zt-fourteenth) ;
#ifdef CUBIC
	a2->qxxx -= k3*(xt*xt*xt - xt*( 9./15 )) ;
	a2->qxxx -= ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a2->qxxx -= kb3*(xt*xt*xt - xt*( 42./(14*16))); 
	a1->qxxx += k3*(xt*xt*xt - xt*( 9./15 )) ;
	a1->qxxx += ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a1->qxxx += kb3*(xt*xt*xt - xt*( 42./(14*16))); 
	a2->qxxy -= k3*(yt*xt*xt - yt*( 6./ 15));
	a2->qxxy -= ka3*(yt*xt*xt - yt*( 11./ 80));
	a2->qxxy -= kb3*(yt*xt*xt - yt*( 17./ (14*16)));
	a1->qxxy += k3*(yt*xt*xt - yt*( 6./ 15));
	a1->qxxy += ka3*(yt*xt*xt - yt*( 11./ 80));
	a1->qxxy += kb3*(yt*xt*xt - yt*( 17./ (14*16)));
	a2->qxxz -= k3*(zt*xt*xt - zt*( 6./ 15));
	a2->qxxz -= ka3*(zt*xt*xt - zt*( 11./ 80));
	a2->qxxz -= kb3*(zt*xt*xt - zt*( 17./ (14*16)));
	a1->qxxz += k3*(zt*xt*xt - zt*( 6./ 15));
	a1->qxxz += ka3*(zt*xt*xt - zt*( 11./ 80));
	a1->qxxz += kb3*(zt*xt*xt - zt*( 17./ (14*16)));
	a2->qxyy -= k3*(yt*yt*xt - xt*( 6./ 15));
	a2->qxyy -= ka3*(yt*yt*xt - xt*( 11./ 80));
	a2->qxyy -= kb3*(yt*yt*xt - xt*( 17./ (14*16)));
	a1->qxyy += k3*(yt*yt*xt - xt*( 6./ 15));
	a1->qxyy += ka3*(yt*yt*xt - xt*( 11./ 80));
	a1->qxyy += kb3*(yt*yt*xt - xt*( 17./ (14*16)));
	a2->qxyz -= (k3+ka3+kb3)*yt*zt*xt;
	a1->qxyz += (k3+ka3+kb3)*yt*zt*xt;
	a2->qxzz -= k3*(zt*zt*xt - xt*( 6./ 15));
	a2->qxzz -= ka3*(zt*zt*xt - xt*( 11./ 80));
	a2->qxzz -= kb3*(zt*zt*xt - xt*( 17./ (14*16)));
	a1->qxzz += k3*(zt*zt*xt - xt*( 6./ 15));
	a1->qxzz += ka3*(zt*zt*xt - xt*( 11./ 80));
	a1->qxzz += kb3*(zt*zt*xt - xt*( 17./ (14*16)));
	a2->qyyy -= k3*(yt*yt*yt - yt*( 9./15 )) ;
	a2->qyyy -= ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a2->qyyy -= kb3*(yt*yt*yt - yt*( 42./(14*16))); 
	a1->qyyy += k3*(yt*yt*yt - yt*( 9./15 )) ;
	a1->qyyy += ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a1->qyyy += kb3*(yt*yt*yt - yt*( 42./(14*16))); 
	a2->qyyz -= k3*(yt*yt*zt - zt*( 6./ 15));
	a2->qyyz -= ka3*(yt*yt*zt - zt*( 11./ 80));
	a2->qyyz -= kb3*(yt*yt*zt - zt*( 17./ (14*16)));
	a1->qyyz += k3*(yt*yt*zt - zt*( 6./ 15));
	a1->qyyz += ka3*(yt*yt*zt - zt*( 11./ 80));
	a1->qyyz += kb3*(yt*yt*zt - zt*( 17./ (14*16)));
	a2->qyzz -= k3*(zt*zt*yt - yt*( 6./ 15));
	a2->qyzz -= ka3*(zt*zt*yt - yt*( 11./ 80));
	a2->qyzz -= kb3*(zt*zt*yt - yt*( 17./ (14*16)));
	a1->qyzz += k3*(zt*zt*yt - yt*( 6./ 15));
	a1->qyzz += ka3*(zt*zt*yt - yt*( 11./ 80));
	a1->qyzz += kb3*(zt*zt*yt - yt*( 17./ (14*16)));
	a2->qzzz -= k3*(zt*zt*zt - zt*( 9./15 )) ;
	a2->qzzz -= ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a2->qzzz -= kb3*(zt*zt*zt - zt*( 42./(14*16))); 
	a1->qzzz += k3*(zt*zt*zt - zt*( 9./15 )) ;
	a1->qzzz += ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a1->qzzz += kb3*(zt*zt*zt - zt*( 42./(14*16))); 
#endif

/* debugging
	j = i *4;
	fprintf(stderr," mxcut %f %f inclose %d who %d \n",mxcut,(*vector)[j+3],inclose,(*atomall)[i]->serial);
	fprintf(stderr," vector %f %f %f \n", (*vector)[j],(*vector)[j+1],(*vector)[j+2]);
*/
	}/* end of loop i */
/* merge the non-bond mxcut lists */
        /* SPEC remove following line which causes compiler warnings.
         * The line was a no-op anyway, since double equals is a 
         * compare.  The author of AMMP says that we should
         * NOT change it to a single equals, as that would "cause a
         * failure under certain odd conditions.  The actual array is
         * preinitialized above to NULL and is filled during these
         * routines" - jh/9/24/99 */
	/* a1->close[inclose] == NULL; */
/* set the position */
	a1->px = a1->dx*lambda + a1->x;
	a1->py = a1->dy*lambda + a1->y;
	a1->pz = a1->dz*lambda + a1->z;

	}  /* end of ii loop */
	
	a_inactive_f_zero();

	free( indexes);
	free( vector);
/*	free( vecold);
*/
	free (atomall);
	return 1;

}


/* f_nonbon()
*
* f_nonbon increments the forces in the atom structures by the force
* due to the nonbon components.  NOTE THE WORD increment.
* the forces should first be zero'd.
* if not then this code will be invalid.  THIS IS DELIBERATE.
* on bigger (and better?) machines the different potential terms
* may be updated at random or in parrellel, if we assume that this routine
* will initialize the forces then we can't do this.
*/
int f_nonbon(lambda)
float lambda;
{
  fv_update_nonbon( lambda);
  float lcutoff,cutoff;
  int inbond,inangle,i,test;
  ATOM *a_next( ); /* returns first ATOM when called with -1 */
  int a_number(),inbuffer,imax;
  float (*buffer)[];
  float (*vector)[];
  ATOM *(*atms)[],*(*atomall)[];
  float dielectric;

  /* first update the lists 
   *  this routine checks if any atom has
   *   broken the mxdq barrier and updates the
   * forces, potentials and expansions thereof */

  dielectric = get_f_variable("dielec");
  if( dielectric < 1.) dielectric = 1.;
  dielectric = 332.17752/dielectric;

  /*  get the number of atoms and allocate the memory for the array space */
  i = a_number();
  atomall = malloc( i*sizeof(ATOM *) );
  if( atomall == NULL)
  {aaerror("cannot allocate memory in f_nonbon"); return 0;}

  imax = a_number();
  for( i=0; i< imax; i++)
  {
    (*atomall)[i] = a_next(i);
  }
  f_nonbon_slice(atomall, dielectric, lambda, 0, 1000);
  f_nonbon_slice(atomall, dielectric, lambda, 1000, 2000);
  f_nonbon_slice(atomall, dielectric, lambda, 2000, 3000);
  f_nonbon_slice(atomall, dielectric, lambda, 3000, 4000);
  f_nonbon_slice(atomall, dielectric, lambda, 5000, 6000);
  f_nonbon_slice(atomall, dielectric, lambda, 7000, 8000);
  f_nonbon_slice(atomall, dielectric, lambda, 8000, 9582);
  a_inactive_f_zero();
  free( atomall); 
  return 1;
}

int f_nonbon_slice(atomall, dielectric, lambda, start, end)
ATOM *(*atomall)[];
float dielectric;
float lambda;
int start;
int end;
/*  returns 0 if error, 1 if OK */
{
  //start_slice();
  ATOM *a1,*a2,*bonded[10],*angled[10];
  float ux,uy,uz;
  float k,r,r0,xt,yt,zt;
  float fx,fy,fz;
  float xt2,xt3,xt4;
  float yt2,yt3,yt4;
  float zt2,zt3,zt4;
  int invector,atomsused,ii,jj;
  void a_inactive_f_zero(void);
  for( int i= start; i< end; i++)
  {
    fx = 0.; fy = 0.; fz = 0.;
    a1 = (*atomall)[i];
    xt = a1->dx*lambda +a1->x - a1->px;
    yt = a1->dy*lambda +a1->y - a1->py;
    zt = a1->dz*lambda +a1->z - a1->pz;


    fx = (a1->qxx*xt + a1->qxy*yt
        + a1->qxz*zt) ;
    fy = (a1->qxy*xt + a1->qyy*yt
        + a1->qyz*zt) ;
    fz = (a1->qxz*xt + a1->qyz*yt
        + a1->qzz*zt) ;
#ifdef CUBIC
    xt2 = xt*xt; yt2 = yt*yt; zt2 = zt*zt;
    fx += a1->qxxx*xt2/2. + a1->qxxy*xt*yt + a1->qxxz*xt*zt
      + a1->qxyy*yt/2. + a1->qxyz*yt*zt + a1->qxzz*zt2/2.;
    fy += a1->qxxy*xt2/2. + a1->qxyy*xt*yt + a1->qxyz*xt*zt
      + a1->qyyy*yt/2. + a1->qyyz*yt*zt + a1->qyzz*zt2/2.;
    fz += a1->qxxz*xt2/2. + a1->qxyz*xt*yt + a1->qxzz*xt*zt
      + a1->qyyz*yt/2. + a1->qyzz*yt*zt + a1->qzzz*zt2/2.;
#endif
#ifdef QUARTIC
    xt3 = xt*xt2; yt3 = yt*yt2; zt3 = zt*zt2;
    fx +=  a1->qxxxx*xt3/6. + a1->qxxxy*xt2*yt/2. + a1->qxxxz*xt2*zt/2.
      + a1->qxxyy*xt*yt/2. + a1->qxxyz*xt*yt*zt + a1->qxxzz*xt*zt2/2.
      + a1->qxyyy*yt3/6. + a1->qxyyz*yt2*zt/2. + a1->qxyzz*yt*zt2/2.
      + a1->qxzzz*zt3/6.;
    fy +=  a1->qxxxy*xt3/6. + a1->qxxyy*xt2*yt/2. + a1->qxxyz*xt2*zt/2.
      + a1->qxyyy*xt*yt/2. + a1->qxyyz*xt*yt*zt + a1->qxyzz*xt*zt2/2.
      + a1->qyyyy*yt3/6. + a1->qyyyz*yt2*zt/2. + a1->qyyzz*yt*zt2/2.
      + a1->qyzzz*zt3/6.;
    fz +=  a1->qxxxz*xt3/6. + a1->qxxyz*xt2*yt/2. + a1->qxxzz*xt2*zt/2.
      + a1->qxyyz*xt*yt/2. + a1->qxyzz*xt*yt*zt + a1->qxzzz*xt*zt2/2.
      + a1->qyyyz*yt3/6. + a1->qyyzz*yt2*zt/2. + a1->qyzzz*yt*zt2/2.
      + a1->qzzzz*zt3/6.;
#endif
#ifdef QUINTIC
    xt4 = xt*xt3; yt4 = yt*yt3; zt4 = zt*zt3;
    fx += a1->qxxxxx*xt4/24. + a1->qxxxxy*xt3*yt/6. + a1->qxxxxz*xt3*zt/6.
      + a1->qxxxyy*xt2*yt2/4. + a1->qxxxyz*xt2*yt*zt/2. + a1->qxxxzz*xt2*zt2/4.
      + a1->qxxyyy*xt*yt3/6. + a1->qxxyyz*xt*yt2*zt/2. + a1->qxxyzz*xt*yt*zt2/2.
      + a1->qxxzzz*xt*zt3/6. + a1->qxyyyy*yt4/24. + a1->qxyyyz*yt3*zt/6. 
      + a1->qxyyzz*yt2*zt2/4. + a1->qxyzzz*yt*zt3/6. + a1->qxzzzz*zt4/24.;
    fy += a1->qxxxxy*xt4/24. + a1->qxxxyy*xt3*yt/6. + a1->qxxxyz*xt3*zt/6.
      + a1->qxxyyy*xt2*yt2/4. + a1->qxxyyz*xt2*yt*zt/2. + a1->qxxyzz*xt2*zt2/4.
      + a1->qxyyyy*xt*yt3/6. + a1->qxyyyz*xt*yt2*zt/2. + a1->qxyyzz*xt*yt*zt2/2.
      + a1->qxyzzz*xt*zt3/6. + a1->qyyyyy*yt4/24. + a1->qyyyyz*yt3*zt/6. 
      + a1->qyyyzz*yt2*zt2/4. + a1->qyyzzz*yt*zt3/6. + a1->qyzzzz*zt4/24.;
    fz += a1->qxxxxz*xt4/24. + a1->qxxxyz*xt3*yt/6. + a1->qxxxzz*xt3*zt/6.
      + a1->qxxyyz*xt2*yt2/4. + a1->qxxyzz*xt2*yt*zt/2. + a1->qxxzzz*xt2*zt2/4.
      + a1->qxyyyz*xt*yt3/6. + a1->qxyyzz*xt*yt2*zt/2. + a1->qxyzzz*xt*yt*zt2/2.
      + a1->qxzzzz*xt*zt3/6. + a1->qyyyyz*yt4/24. + a1->qyyyzz*yt3*zt/6. 
      + a1->qyyzzz*yt2*zt2/4. + a1->qyzzzz*yt*zt3/6. + a1->qzzzzz*zt4/24.;
#endif
    a1->fx += fx  + a1->dpx;
    a1->fy += fy  + a1->dpy;
    a1->fz += fz  + a1->dpz;
    /* do the close atoms */
    for( jj=0; jj< NCLOSE; jj++)
    { if( a1->close[jj] == NULL) break; }
    for( ii=0; ii< jj;ii++)
    {
      a2 = a1->close[ii]; 
      /* note ux is backwards from below */
      ux = (a2->dx -a1->dx)*lambda +(a2->x -a1->x);
      uy = (a2->dy -a1->dy)*lambda +(a2->y -a1->y);
      uz = (a2->dz -a1->dz)*lambda +(a2->z -a1->z);
      r =one/( ux*ux + uy*uy + uz*uz); r0 = sqrt(r);
      ux = ux*r0; uy = uy*r0; uz = uz*r0;
      k = -dielectric*a1->q*a2->q*r;
      r = r*r*r;
      k += a1->a*a2->a*r*r0*six;
      k -= a1->b*a2->b*r*r*r0*twelve;
      a1->fx += ux*k;
      a1->fy += uy*k;
      a1->fz += uz*k;
      a2->fx -= ux*k;
      a2->fy -= uy*k;
      a2->fz -= uz*k;
    }
  } 

  //end_slice();
  return 1;
}
/* v_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int v_nonbon( V, lambda )
        float *V,lambda;
{
        float r,r0,xt,yt,zt;
        float k;
        int inbond,inangle,i;
        ATOM *a1,*a2,*bonded[10],*angled[10];
        ATOM *a_next( ); /* returns first ATOM when called with -1 */
        int a_number(),inbuffer;
        int invector,atomsused,ii,jj,imax;
        float (*vector)[];
        float vx;
        float k2;
        ATOM *(*atomall)[];
        float dielectric;
	float xt2,xt3,xt4,xt5;
	float yt2,yt3,yt4,yt5;
	float zt2,zt3,zt4,zt5;

	fv_update_nonbon( lambda);

        dielectric = get_f_variable("dielec");
        if( dielectric < 1.) dielectric = 1.;
        dielectric = 332.17752/dielectric;

/*  get the number of atoms and allocate the memory for the array space */
        i = a_number();
        atomall = malloc( i*sizeof(ATOM *) );
        if( atomall == NULL)
        {aaerror("cannot allocate memory in v_nonbon"); return 0;}

        imax = a_number();
        for( i=0; i< imax; i++)
        {
                (*atomall)[i] = a_next(i);
        }
        for( i= 0; i< imax; i++)
        {
                a1 = (*atomall)[i];
		vx = a1->VP;
                xt = a1->dx*lambda +a1->x - a1->px;
                yt = a1->dy*lambda +a1->y - a1->py;
                zt = a1->dz*lambda +a1->z - a1->pz;
                vx -= (a1->dpx*xt + a1->dpy*yt
                        + a1->dpz*zt) ;
                vx -= ( (xt*(.5*a1->qxx*xt + a1->qxy*yt + a1->qxz*zt)
                + yt*(.5*a1->qyy*yt + a1->qyz*zt) + .5*zt*a1->qzz*zt));
#ifdef CUBIC
		xt2 = xt*xt; yt2 = yt*yt;  zt2 = zt*zt;
		xt3 = xt2*xt; yt3 = yt2*yt; zt3 = zt2*zt;

	vx -= a1->qxxx*xt3/6. + a1->qxxy*xt2*yt/2 + a1->qxxz*xt2*zt/2
		+ a1->qxyy*xt*yt2/2 + a1->qxyz*xt*yt*zt + a1->qxzz*xt*zt2/2
		+ a1->qyyy*yt3/6 + a1->qyyz*yt2*zt/2 + a1->qyzz*yt*zt2/2 
		+ a1->qzzz*zt3/6.;
#endif
#ifdef QUARTIC
		xt4 = xt3*xt; yt4 = yt3*yt; zt4 = zt3*zt;
	vx -= a1->qxxxx*xt4/24. + a1->qxxxy*xt3*yt/6. + a1->qxxxz*xt3*yt/6. + a1->qxxyy*xt2*yt2/4.
		+ a1->qxxyz*xt2*yt*zt/2. + a1->qxxzz*xt2*zt2/4. + a1->qxyyy*xt*yt3/6.
		+ a1->qxyyz*xt*yt2*zt/2. + a1->qxyzz*xt*yt*zt2/2. + a1->qxzzz*xt*zt3/6.
		+ a1->qyyyy*yt4/24. + a1->qyyyz*yt3*zt/6. + a1->qyyzz*yt2*zt2/4. + a1->qyzzz*yt*zt3/6.
		+ a1->qzzzz*zt4/24.;
#endif
#ifdef QUINTIC
		xt5 = xt4*xt; yt5 = yt4*yt; zt5 = zt4*zt;
	vx -= a1->qxxxxx*xt5/120. + a1->qxxxxy*xt4*yt/24. + a1->qxxxxz*xt4*zt/24.
		+ a1->qxxxyy*xt3*yt2/12. + a1->qxxxyz*xt3*yt*zt/6. + a1->qxxxzz*xt3*zt2/12.
		+ a1->qxxyyy*xt2*yt3/12. + a1->qxxyyz*xt2*yt2*zt/4. + a1->qxxyzz*xt2*yt*zt2/4.
		+ a1->qxxzzz*xt2*zt3/12. + a1->qxyyyy*xt*yt4/24.  + a1->qxyyyz*xt*yt3*zt/6.
		+ a1->qxyyzz*xt*yt2*zt2/4. + a1->qxyzzz*xt*yt*zt3/6. + a1->qxzzzz*xt*zt4/24.
		+ a1->qyyyyy*yt5/120. + a1->qyyyyz*yt4*zt/24 + a1->qyyyzz*yt3*zt2/12.
		+ a1->qyyzzz*yt2*zt3/12. + a1->qyzzzz*yt*zt4/24. + a1->qzzzzz*zt5/120.;

#endif
/* do the close atoms */
	for( jj=0; jj< NCLOSE; jj++)
	{ if( a1->close[jj] == NULL) break; }
        for( ii=0; ii< jj;ii++)
        {
        a2 = a1->close[ii]; 
        xt = (a2->dx -a1->dx)*lambda +(a2->x -a1->x);
        yt = (a2->dy -a1->dy)*lambda +(a2->y -a1->y);
        zt = (a2->dz -a1->dz)*lambda +(a2->z -a1->z);
        r = one/(xt*xt + yt*yt + zt*zt); r0 = sqrt(r);
/*      xt = xt/r0; yt = yt/r0; zt = zt/r0;
*/
        k = dielectric*a1->q*a2->q*r0;
        r = r*r*r;
        k -= a1->a*a2->a*r;
        k += a1->b*a2->b*r*r;
	vx += k;
        }
        *V += vx;
	}
	a_inactive_f_zero();
        free( atomall); return 1;
}
ATOM *a_next( int flag ); /* returns first ATOM when called with -1 */
ATOM *a_m_serial( int serial );
/*zone_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int zone_nonbon( V, lambda ,alist, inalist )
	float *V,lambda;
	ATOM *( *alist)[] ;
	int inalist;
{
	float r,r0,xt,yt,zt;
	float lcutoff,cutoff;
	int inbond,inangle,i,ii;
	ATOM *a1,*a2;
	float dielectric,ve,va,vh;
  float get_f_variable(char *name);

/* nonbonded potentials 
* do a double loop starting from the first atom to the 
* last 
* then from the second to the last 
* etc
*
* also check to avoid bonded and 1-3 bonded atoms
*/
	if( inalist <= 0 ) return 1;
	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	cutoff = get_f_variable("cutoff");
	if( cutoff < 1.) cutoff = 1.e10;
	lcutoff = -cutoff;
	for( ii=0; ii< inalist; ii++)
	{
	a1 = (*alist)[ii];
	if( a1 == NULL ) goto NOTANATOM;
	ve = 0.; va = 0.; vh = 0.;
	a2 = a_next(-1);
/*	
*	for(i = 0; i< a1->dontuse; i++)
*	printf("%d ",a1->excluded[i]->serial);
*	printf("\n");
*/
/*
	while(  (a2->next != a2) && (a2->next != NULL))
	*/
	while(  (a2 != NULL) && (a2->next != NULL) && a2->next != a2)
	{
/* goto SKIP is used because this is one case where it makes sense */
/*	if( a2 == a1) break;  */
	if( a2 == a1) goto SKIP;
	for(i = 0; i< a1->dontuse; i++)
		if( a2 == a1->excluded[i]) goto SKIP;
/* non - bonded are only used when the atoms arent bonded */

	xt = (a1->x -a2->x) + lambda*(a1->dx - a2->dx);
	if( (xt > cutoff) || (xt < lcutoff) ) goto SKIP;
	yt = (a1->y -a2->y) + lambda*(a1->dy - a2->dy);
	if( (yt > cutoff) || (yt < lcutoff) ) goto SKIP;
	zt = (a1->z -a2->z) + lambda*(a1->dz - a2->dz);
	if( (zt > cutoff) || (zt < lcutoff) ) goto SKIP;

	r = xt*xt+yt*yt+zt*zt;
	if( r < 1.) r = 1.;  

	r0 = sqrt(r); r = r*r*r ;
/* debugging
	printf(" %d %d %f %f %f \n", a1->serial,a2->serial,a1->q,a2->q,
	332.17752*a1->q*a2->q/r0);
*/
	ve += dielectric*a1->q*a2->q/r0; 
	va -= a1->a*a2->a/r;
	vh += a1->b*a2->b/r/r; 

SKIP:
/*	if( a2->next == a1) break; */
	if( a2->next == a2) break;  
	a2 = a2->next;
	}
	*V += ve + va + vh;
NOTANATOM:
	i = i;
	}
	return 1;

}

