/* torsion.c
*
* collection of routines to service bond torsion  potentials
*
* POOP (Poor-mans Object Oriented Programming) using scope rules
*
* these routines hold a data base (in terms of array indeces)
* of torsion, with the associated length and force constant
*
* (this could be table driven but what the hell memories cheap)
*
* the routines for potential value, force and (eventually) second
* derivatives are here also
*
* force and 2nd derivative routines assume zero'd arrays for output
* this allows for parralellization if needed (on a PC?)
*
* forces are bond wise symmetric - so we don't have to mess around with
* s matrices and the like.
*/
/*
*  copyright 1992 Robert W. Harrison
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

#define ANSI 1
/* misc includes - ANSI and some are just to be safe */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef ANSI
#include <stdlib.h>
#endif
/* global defs */
#define TOO_SMALL 1.e-8
#define doublefloat double

#include "ammp.h"
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-torsioned potentials
*/
typedef struct{
	ATOM *atom1,*atom2,*atom3,*atom4;
	float k,offset;
	int   n;
	void *next;
	}  TORSION;
#define TLONG sizeof(TORSION)

TORSION *torsion_first = NULL;
TORSION *torsion_last = NULL;
float bond_length(ATOM * a1, ATOM *a2);
/* function torsion adds a torsion to the torsion list
* returns 1 if ok
* returns 0 if not
*  is passed the array pointers, length and constant
* allocates the new memory, initializes it and
* returns
*/
int torsion( p1,p2,p3,p4,fk,n,off)
	int p1,p2,p3,p4,n;
	float fk,off;
	{
	TORSION *new;
	ATOM *ap1,*ap2,*ap3,*ap4,*a_m_serial();
	char line[80];
/* consistancy checks */
	if(  p1 == p2 || p1 == p3 || p1 == p4
	|| p2 == p3 || p2 == p4 || p3 == p4)
	{
	sprintf( line," same atom in torsion %d %d %d %d \0",p1,p2,p3,p4);
	aaerror( line );
	return 1;
	}
	if( fk == 0.) return 1;
/* get the atom pointers for the two serial numbers */
	ap1 = a_m_serial( p1 );
	ap2 = a_m_serial( p2 );
	ap3 = a_m_serial( p3 );
	ap4 = a_m_serial( p4 );
	if( (ap1 == NULL) || (ap2 == NULL) || (ap3==NULL) || (ap4==NULL) ) 
	{
	sprintf( line,"undefined atom in torsion %d %d %d %d \0",p1,p2,p3,p4);
	aaerror( line );
	return 0;
	}

	if( ( new = malloc( TLONG ) ) == NULL)
	{
	return 0;
	}
	/* initialize the pointers */
	if( torsion_first == NULL) torsion_first = new;
	if( torsion_last == NULL) torsion_last = new;
	new -> atom1 = ap1;
	new -> atom2 = ap2;
	new -> atom3 = ap3;
	new -> atom4 = ap4;
	new -> offset = off;
	new -> k = fk;
	new -> n = n;
	new -> next = new;
	torsion_last -> next = new;
	torsion_last = new;
	return 1;
	}


/* v_torsion()
* this function sums up the potentials
* for the atoms defined in the torsion data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int v_torsion( V, lambda )
	float *V,lambda;
{
	TORSION *bp;
/* difference vectors */
	doublefloat x1,y1,z1,x2,y2,z2,x3,y3,z3;
/* cross products and storage for normalizing */
	doublefloat r,cx1,cy1,cz1,cx2,cy2,cz2;
	doublefloat dp;
	ATOM *a1,*a2,*a3,*a4;


	bp = torsion_first;
       if( bp == NULL ) return 1;
       while(1)
       {
	if( bp == NULL) return 0;
	a1 = bp->atom1; a2 = bp->atom2; a3 = bp->atom3;
	a4 = bp->atom4;
	if( a1->active|| a2->active || a3->active || a4->active) {
	x1 = (a1->x -a2->x +lambda*(a1->dx-a2->dx));
	y1 = (a1->y -a2->y +lambda*(a1->dy-a2->dy));
	z1 = (a1->z -a2->z +lambda*(a1->dz-a2->dz));
	x2 = (a3->x -a2->x +lambda*(a3->dx-a2->dx));
	y2 = (a3->y -a2->y +lambda*(a3->dy-a2->dy));
	z2 = (a3->z -a2->z +lambda*(a3->dz-a2->dz));
	x3 = (a4->x -a3->x +lambda*(a4->dx-a3->dx));
	y3 = (a4->y -a3->y +lambda*(a4->dy-a3->dy));
	z3 = (a4->z -a3->z +lambda*(a4->dz-a3->dz));
/* 1 cross 2 */
	cx1 = y1*z2 - y2*z1;
	cy1 = -x1*z2 + x2*z1;
	cz1 = x1*y2 - x2*y1;
	r = cx1*cx1 + cy1*cy1 + cz1*cz1;
	if( r < TOO_SMALL) goto SKIP;
	r = one/sqrt(r);
	cx1 = cx1*r;
	cy1 = cy1*r;
	cz1 = cz1*r;
/* 3 cross 2 */
	cx2 = y3*z2 - y2*z3;
	cy2 = -x3*z2 + x2*z3;
	cz2 = x3*y2 - x2*y3;
	r = cx2*cx2 + cy2*cy2 + cz2*cz2;
	if( r < TOO_SMALL) goto SKIP;
	r = one/sqrt(r);
	cx2 = cx2*r;
	cy2 = cy2*r;
	cz2 = cz2*r;
/* if here everything is well determined */
	dp = cx1*cx2 + cy1*cy2 + cz1*cz2; /* cos( abs(theta)) */
	if( dp > 1.) dp = 1.; if( dp < -1.) dp = -1.;
	dp = acos(dp);
/* determine the sign by triple product */
	r = cx1*x3 + cy1*y3 + cz1*z3;
	if( r > 0 ) dp =  -dp ;
	*V += .5*(bp->k)*( 1. + cos( bp->n * dp - bp->offset   )) ; 
	}
SKIP:
	if( bp == bp->next ) return 1;
	bp = bp->next;
       }
}
/* f_torsion()
*
* f_torsion increments the forces in the atom structures by the force
* due to the torsion components.  NOTE THE WORD increment.
* the forces should first be zero'd.
* if not then this code will be invalid.  THIS IS DELIBERATE.
* on bigger (and better?) machines the different potential terms
* may be updated at random or in parrellel, if we assume that this routine
* will initialize the forces then we can't do this.
*/
int f_torsion(lambda)
float lambda;
/*  returns 0 if error, 1 if OK */
{
//  start_slice();
	TORSION *bp;
/* difference vectors */
	doublefloat x1,y1,z1,x2,y2,z2,x3,y3,z3;
/* cross products and storage for normalizing */
	doublefloat r,cx1,cy1,cz1,cx2,cy2,cz2;
	doublefloat dp,sdp;
	doublefloat r1,r2,r3,c1,c2,s1,s2;
	doublefloat ux,uy,uz;
	int i;	
	ATOM *a1,*a2,*a3,*a4;


	bp = torsion_first;
       if( bp == NULL ) return 1;
       i = 0;
       while(1)
       {
/* debugging a mysterious error	
* there was a crash at the a1= line
* seems to have been when atom1 and atom2 where the
* same
	printf(" torsion %d\n",i++);
	printf(" %s\n",bp->atom1->name);
	printf(" %s\n",bp->atom2->name);
	printf(" %s\n",bp->atom3->name);
	printf(" %s\n",bp->atom4->name);
*/
	if( bp == NULL) return 0;
	a1 = bp->atom1; a2 = bp->atom2; a3 = bp->atom3;
	a4 = bp->atom4;
	if( a1->active|| a2->active || a3->active || a4->active) {
	x1 = (a1->x -a2->x +lambda*(a1->dx-a2->dx));
	y1 = (a1->y -a2->y +lambda*(a1->dy-a2->dy));
	z1 = (a1->z -a2->z +lambda*(a1->dz-a2->dz));
	x2 = (a3->x -a2->x +lambda*(a3->dx-a2->dx));
	y2 = (a3->y -a2->y +lambda*(a3->dy-a2->dy));
	z2 = (a3->z -a2->z +lambda*(a3->dz-a2->dz));
	x3 = (a4->x -a3->x +lambda*(a4->dx-a3->dx));
	y3 = (a4->y -a3->y +lambda*(a4->dy-a3->dy));
	z3 = (a4->z -a3->z +lambda*(a4->dz-a3->dz));
/* lengths for normalization */
	r1 = sqrt( x1*x1 + y1*y1 + z1*z1);
	r2 = sqrt( x2*x2 + y2*y2 + z2*z2);
	r3 = sqrt( x3*x3 + y3*y3 + z3*z3);
	c1 = (x1*x2 + y1*y2 + z1*z2)/r1/r2;
	c2 = -(x2*x3 + y2*y3 + z2*z3)/r2/r3;
	/*
	s1 = sqrt( 1. - c1*c1); s2 = sqrt( 1. -c2*c2);
	*/
	s1 = ( 1. - c1*c1); s2 = ( 1. -c2*c2);
	if( s1 < TOO_SMALL) goto SKIP;
	if( s2 < TOO_SMALL) goto SKIP;
/* 1 cross 2 */
	cx1 = y1*z2 - y2*z1;
	cy1 = -x1*z2 + x2*z1;
	cz1 = x1*y2 - x2*y1;
	r = cx1*cx1 + cy1*cy1 + cz1*cz1;
	if( r < TOO_SMALL) goto SKIP;
	r = one/sqrt(r);
	cx1 = cx1*r;
	cy1 = cy1*r;
	cz1 = cz1*r;
/* 3 cross 2 */
	cx2 = y3*z2 - y2*z3;
	cy2 = -x3*z2 + x2*z3;
	cz2 = x3*y2 - x2*y3;
	r = cx2*cx2 + cy2*cy2 + cz2*cz2;
	if( r < TOO_SMALL) goto SKIP;
	r = one/sqrt(r);
	cx2 = cx2*r;
	cy2 = cy2*r;
	cz2 = cz2*r;
/* if here everything is well determined */
	dp = cx1*cx2 + cy1*cy2 + cz1*cz2; /* cos( abs(theta)) */
	if( dp > 1.) dp = 1.; if( dp < -1.) dp = -1.;
	dp = acos(dp);
/* determine the sign by triple product */
/*	r = cx1*x3 + cy1*y3 + cz1*z3;  */
	r = sqrt(x3*x3 +y3*y3 +z3*z3)*
	    sqrt(x2*x2+y2*y2+z2*z2);
	sdp = x3*x2 + y3*y2 + z3*z2;
	sdp = sdp/r;
	ux = x3 - sdp*x2;
	uy = y3 - sdp*y2;
	uz = z3 - sdp*z2;
	r = cx1*ux + cy1*uy + cz1*uz;

	if( r >= 0 ) dp = -dp  ; 

/*
*	printf(" dp target %f %f %f %f\n",dp,bp->n*dp-bp->offset,
*	-sin(bp->n*dp-bp->offset),1.+cos(bp->n*dp-bp->offset));
*/

/* the potential */
/*	*V += .5*(bp->k)*( 1. + cos( bp->n * dp -bp->offset  )) ;  */
/* its derivative */
	r = -.5*bp->k*bp->n*sin( bp->n*dp - bp->offset);
/*
	if( bp->n == 1)
	{
	r = -.5*bp->k*sin(dp);
	} else if( bp->n == 2)
	{
	r = -bp->k*(sin(dp+dp));
	} else if( bp->n == 3)
	{
	r = cos(dp);
	r = -.5*(12*r*r-3)*sin(dp);
	} else
	{
	r =   -.5*(bp->k)*( (bp->n)*sin(bp->n*dp  ));
	}
	if( bp->offset > 1.e-5 || bp->offset < -1.e-5)
		r = -r;
*/	
	s1 = one/s1;
	s2 = one/s2;

	if( a1->active){
	a1->fx -= r*cx1/r1*s1;
	a1->fy -= r*cy1/r1*s1;
	a1->fz -= r*cz1/r1*s1;
	}

	if( a2->active){
	a2->fx += r*cx1*(r2-c1*r1)/r2/r1*s1;
	a2->fy += r*cy1*(r2-c1*r1)/r2/r1*s1;
	a2->fz += r*cz1*(r2-c1*r1)/r2/r1*s1;
	a2->fx -= r*cx2*c2/r2*s2;
	a2->fy -= r*cy2*c2/r2*s2;
	a2->fz -= r*cz2*c2/r2*s2;
	}

	if( a3->active){
	a3->fx -= r*cx2*(r2-c2*r3)/r2/r3*s2;
	a3->fy -= r*cy2*(r2-c2*r3)/r2/r3*s2;
	a3->fz -= r*cz2*(r2-c2*r3)/r2/r3*s2;
	a3->fx += r*cx1*c1/r2*s1;
	a3->fy += r*cy1*c1/r2*s1;
	a3->fz += r*cz1*c1/r2*s1;
	}

	if( a4->active){
	a4->fx += r*cx2/r3*s2;
	a4->fy += r*cy2/r3*s2;
	a4->fz += r*cz2/r3*s2;
	}


	}
SKIP:
	if( bp == bp->next ) {
//    end_slice();
    return 1;
  }
	bp = bp->next;
       }
}
/* function get_torsion( a1,bonded,10,inbond);
* check the torsion list for atoms 1-4 ed to a1
*/
void get_torsion( a1,bonded,mbond,inbond)
ATOM *a1, *bonded[];
int mbond,*inbond ;
{
	TORSION *mine;
	mine = torsion_first;
	*inbond = 0;
	while(1)
	{
	if( (mine == NULL) )
	{
		return;
	}
	if( mine->atom1 == a1)
	{
		bonded[(*inbond)++] = mine->atom4;
	}
	if( mine->atom4 == a1)
	{
		bonded[(*inbond)++] = mine->atom1;
	}
	if( mine == mine->next) return;
	mine = mine->next;
	if( *inbond == mbond ) return;
	}		
}
/* routine dump_torsions
* this function outputs the torsion parameters
* and does it in a simple form
* torsion ser1,ser2,ser3,k,theta (in degrees )
* the rest is just free format
*/
/* SPEC use modern style declaration, to match proto jh/9/22/99 */
void dump_torsions( FILE *where )
{
	TORSION *b;
	ATOM *a1,*a2,*a3,*a4;
	float rtodeg;
	b = torsion_first;
	if( b == NULL ) return;
	rtodeg = 180./acos(-1.);
	while( (b->next != b)  )
	{
	if( b->next == NULL) return;
	a1 = b->atom1; a2 = b->atom2;a3 = b->atom3; a4 = b->atom4;
	fprintf( where,"torsion %d %d %d %d %f %d %f ;\n",
		a1->serial,a2->serial,
		a3-> serial,a4->serial,b->k,b->n,b->offset*rtodeg);
	b = b->next;
	}
	if( b->next == NULL) return;
	a1 = b->atom1; a2 = b->atom2;a3 = b->atom3; a4 = b->atom4;
	fprintf( where,"torsion %d %d %d %d %f %d %f ;\n",
		a1->serial,a2->serial,
		a3-> serial,a4->serial,b->k,b->n,b->offset*rtodeg);
}	

/* a_torsion()
* this function sums up the potentials
* for the atoms defined in the torsion data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int a_torsion( V, lambda ,ilow,ihigh,op)
	float *V,lambda;
	int ilow,ihigh;
	FILE *op;
{
	TORSION *bp;
/* difference vectors */
	float x1,y1,z1,x2,y2,z2,x3,y3,z3;
/* cross products and storage for normalizing */
	float r,cx1,cy1,cz1,cx2,cy2,cz2;
	float dp;
	ATOM *a1,*a2,*a3,*a4;


	bp = torsion_first;
       if( bp == NULL ) return 1;
       while(1)
       {
	if( bp == NULL) return 0;
	a1 = bp->atom1; a2 = bp->atom2; a3 = bp->atom3;
	a4 = bp->atom4;
	if( (a1->serial >= ilow && a1->serial <= ihigh)
	||  (a2->serial >= ilow && a2->serial <= ihigh)
	||  (a3->serial >= ilow && a3->serial <= ihigh) 
	||  (a4->serial >= ilow && a4->serial <= ihigh) )
	{
	x1 = (a1->x -a2->x +lambda*(a1->dx-a2->dx));
	y1 = (a1->y -a2->y +lambda*(a1->dy-a2->dy));
	z1 = (a1->z -a2->z +lambda*(a1->dz-a2->dz));
	x2 = (a3->x -a2->x +lambda*(a3->dx-a2->dx));
	y2 = (a3->y -a2->y +lambda*(a3->dy-a2->dy));
	z2 = (a3->z -a2->z +lambda*(a3->dz-a2->dz));
	x3 = (a4->x -a3->x +lambda*(a4->dx-a3->dx));
	y3 = (a4->y -a3->y +lambda*(a4->dy-a3->dy));
	z3 = (a4->z -a3->z +lambda*(a4->dz-a3->dz));
/* 1 cross 2 */
	cx1 = y1*z2 - y2*z1;
	cy1 = -x1*z2 + x2*z1;
	cz1 = x1*y2 - x2*y1;
	r = cx1*cx1 + cy1*cy1 + cz1*cz1;
	if( r < TOO_SMALL) goto SKIP;
	r = sqrt(r);
	cx1 = cx1/r;
	cy1 = cy1/r;
	cz1 = cz1/r;
/* 3 cross 2 */
	cx2 = y3*z2 - y2*z3;
	cy2 = -x3*z2 + x2*z3;
	cz2 = x3*y2 - x2*y3;
	r = cx2*cx2 + cy2*cy2 + cz2*cz2;
	if( r < TOO_SMALL) goto SKIP;
	r = sqrt(r);
	cx2 = cx2/r;
	cy2 = cy2/r;
	cz2 = cz2/r;
/* if here everything is well determined */
	dp = cx1*cx2 + cy1*cy2 + cz1*cz2; /* cos( abs(theta)) */
	if( dp > 1.) dp = 1.; if( dp < -1.) dp = -1.;
	dp = acos(dp);
/* determine the sign by triple product */
	r = cx1*x3 + cy1*y3 + cz1*z3;
	if( r > 0 ) dp =  -dp ;
	z2 = .5*(bp->k)*( 1. + cos( bp->n * dp - bp->offset   )) ; 
	*V += z2;
	fprintf(op,"Torsion %s %d %s %d %s %d %s %d E %f Angle %f error %f\n"
		,a1->name,a1->serial,a2->name,a2->serial,a3->name,a3->serial,
		a4->name,a4->serial, z2,dp*180./3.14159265
	,acos(-(cos(bp->n*dp-bp->offset)))*180./3.14159265 );
	}
SKIP:
	if( bp == bp->next ) return 1;
	bp = bp->next;
       }
}
gsdg_torsion( ap )
ATOM *ap;
{
TORSION *tp;
float b1,b2,b3;

	tp =torsion_first;
	while( 1)
	{ 
		if( tp == NULL ) return 0;
		if( tp->atom1 == ap)
		{
			b1 = bond_length(ap,tp->atom2);
			b2 = bond_length(tp->atom2,tp->atom3);
			b3 = bond_length(tp->atom3,tp->atom4);
			ap->vx = b2 + .75*(b1+b3);
			ap->vx *= ap->vx;
			ap->vy = 10.;
		}
		if( tp->atom4 == ap)
		{
			b1 = bond_length(ap,tp->atom3);
			b2 = bond_length(tp->atom2,tp->atom3);
			b3 = bond_length(tp->atom2,tp->atom1);
			ap->vx = b2 + .75*(b1+b3);
			ap->vx *= ap->vx;
			ap->vy = 10.;
		}
		if( tp == tp->next) return 0;
		tp = tp->next;
	}
}
