/*  animate.c
*
* routines for performing molecular dynamics
*
* v_maxwell( float Temperature, driftx,drifty,driftz)  initialize velocities
*                           with maxwell distribution, assuming
*                           simple kinetic theory relating T and v
*                           driftx,drifty,driftz allow the use of a constant 
*                           drift velocity.
*
* int v_rescale( float Temperature)
*  rescale velocities so that ke is 3RT/2M
*
* int verlet(forces,nforces, nstep,dtime)
*           perform verlet (forward Euler) dynamics
*                           
* int pac( forces,nforces, nstep,dtime)
*            predict and correct dynamics
*
* int pacpac( forces,nforces,nstep,dtime)
*             iterated pac dynamics
*
*
* int tpac( forces,nforces, nstep,dtime, T)
*  nose constrained dynamics
* int ppac( forces,nforces, nstep,dtime, P)
*   pressure only constrained
* int ptpac( forces,nforces, nstep,dtime,P, T)
*   pressure and temperature constrained
* int hpac( forces,nforces, nstep,dtime, H)
*  total energy  constrained dynamics
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

#define ANSI 1
/* misc includes - ANSI and some are just to be safe */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef ANSI
#include <stdlib.h>
#endif
#include "ammp.h"
int a_inc_v( float lambda );
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-bonded potentials
*/
float randf()
{
  static float buff[55];
  static int ip = 0,jp = 0,kp;
  int i,seed;
   float xva;
  if( ip == 0 && jp == 0)
  {
/* retrieve the seed from the storage */
  seed = get_i_variable("seed");
/* initialize when the pointers are both zero   */
  for( ip=0; ip < 55; ip++)
      { seed = (seed*2349+14867)%32767;
    buff[ip] = (float)seed/32767.;
    if( buff[ip] > 1.) buff[ip] = buff[ip]-1.;
    if( buff[ip] < 0.) buff[ip] = buff[ip]+1.;
    }
  ip = 24; jp=55-ip; kp = 0;
  }
  i = kp;
  xva = buff[jp]+buff[ip];
  if( xva > 1.) xva = xva -1.;
  buff[kp] = xva;
  kp = (kp+1)%55;
  ip = (ip+1)%55;
  jp = (jp+1)%55;
  return buff[i];
}

float randg()
{
  float x1,x2,norm;

  norm = 2.;
  while( norm > 1.)
  {
    x1 = 2.*randf()-1; x2 = 2.*randf()-1;
    norm = x1*x1 + x2*x2;
  }

  if( norm < 1.e-9) norm = 1.e-9;
  return x1*sqrt( -2.*log(norm)/norm );
}
void rand3( float *x, float *y, float *z )
{
  float alpha,norm,x1,x2;
  norm = 2.;
  while( norm > 1.)
  {
    x1 = 2.*randf()-1; x2 = 2.*randf()-1;
    norm = x1*x1 + x2*x2;
  }
/*  alpha = 2.*sqrt(1.-norm);
  *x = x1*alpha;
  *y = x2*alpha;
  *z = 2.*norm-1.;
*/
  *x = x1; *y = x2;
  norm = sqrt(1.-norm);
  *z = norm;
  if( randf() < 0.5) *z = -norm;
}

int v_maxwell( T,dx,dy,dz)
float T,dx,dy,dz;
{
	/* void rand3();   SPEC declare proto in ammp.h jh/9/22/99   */
	ATOM *ap,*a_next(),*bonded[10];
	int iflag,inbond;
	float vmag;
	float R;
	R = 1.987 ; /* kcal/mol/K */

	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.)
	{
/* convert from kcal to mks */
/* 4.184 to joules */
/* 1000 grams to kg */
	vmag = sqrt( 3.*R*T/ap->mass*4.184*1000.)* randg();
	rand3( &ap->vx,&ap->vy,&ap->vz);
	if( ap->active ){
	ap->vx = ap->vx*vmag + dx;
	ap->vy = ap->vy*vmag + dy;
	ap->vz = ap->vz*vmag + dz;
	}else{ 
	ap->vx = 0.;
	ap->vy = 0.;
	ap->vz = 0.;
	}
	}
	}
/* now check those who are zero mass */
/* and give them the velocity of the first bonded atom */
	iflag = -1;
	while( (ap= a_next(iflag)) != NULL)
	{
	iflag = 1;
		if( ap->mass <= 0.)
		{
		get_bond(ap,bonded,10,&inbond);
			if( inbond >= 0)
			{
			ap->vx = bonded[0]->vx;
			ap->vy = bonded[0]->vy;
			ap->vz = bonded[0]->vz;
			}
		}
	}
	return 1;
}
/* v_rescale(T)
*  rescale the velocities for constant KE  == Temperature 
*/
int v_rescale( T )
float T;
{
	ATOM *ap,*a_next();
	int iflag,a_number();
	float vmag,KE,target;
	float R;
	R = 1.987 ; /* kcal/mol/K */

	target = 0.;
	target += .5*(3.*R*T)*4.184*1000*a_number();
	KE = 0.; 
	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.)
	{
	vmag = ap->vx*ap->vx+ap->vy*ap->vy+ap->vz*ap->vz;
	KE += ap->mass*vmag;	
	}}
	KE = KE *.5;
	if( KE == 0.)
	{ aaerror(" Cannot rescale a zero velocity field -use v_maxwell");
		return 0;
		}
	vmag = sqrt(target/KE);

	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	ap->vx = ap->vx*vmag;
	ap->vy = ap->vy*vmag;
	ap->vz = ap->vz*vmag;
	}
	return 1;
}
/* routine verlet( nstep,dtime)
*int verlet(forces,nforces, nstep,dtime)
*
* perform nstep leapfrogging dynamics with dtime
*/
int verlet(forces,nforces, nstep,dtime)
int (*forces[])(),nforces;
int nstep;
float dtime;
{
	ATOM *bp,*ap,*a_next(),*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( 0.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->vx += ap->fx/ap->mass*dtime*4.184e6;
		ap->vy += ap->fy/ap->mass*dtime*4.184e6;
		ap->vz += ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	a_inc_v(dtime);
	}/* end of istep loop */
	return 1;
}
/* routine pac( nstep,dtime)
*int pac(forces,nforces, nstep,dtime)
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int pac(forces,nforces, nstep,dtime)
int (*forces[])(),nforces;
int nstep;
float dtime;
{
	ATOM *ap,*bp,*a_next(),*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
/*		ap->vx += ap->fx/ap->mass*dtime*4.184e6/6.;
*		ap->vy += ap->fy/ap->mass*dtime*4.184e6/6.;
*		ap->vz += ap->fz/ap->mass*dtime*4.184e6/6.;
*/
		ap->vx =  ap->dx +  ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine tpac( nstep,dtime)
*int tpac(forces,nforces, nstep,dtime,T)
*
* perform nstep pac dynamics with dtime
* kinetic energy constraint to (3*natom-1) kT/2
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*
* adaptive steps (6/19/96)
*  if the rescale is too large (i.e. > 2) do two half steps
*
*/
int tpac(forces,nforces, nstep,dtime_real,T)
int (*forces[])(),nforces;
int nstep;
float dtime_real,T;
{
	ATOM *ap,*bp,*a_next(),*bonded[10];
	float ke,Tke,R;
	float alpha;
	float dtime;
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */
	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	ke = 0.;
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ke += ap->mass*(
	ap->vx*ap->vx + ap->vy*ap->vy + ap->vz*ap->vz);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}
	Tke = 3*(imax)*R*4.184*1000;  /* converted into MKS */
	Tke = ke/Tke;  /* Tke is now the current temperature */ 
/* scale the current velocities */
	dtime = dtime_real;
	if( Tke > 1.e-6)
	{	
	ke = sqrt(T/Tke); /* ke is the scaled shift value */
	dtime = dtime_real/ke;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
		tpac(forces,nforces,1,dtime_real*0.5,T); 
		tpac(forces,nforces,1,dtime_real*0.5,T); 
		goto SKIP;
			}
	ap = a_next(-1);
	bp =  ap;
	for( i=0; i< imax;  i++, ap = bp)
	{
	bp = a_next(1);
	ap->dx *= ke;
	ap->dy *= ke;
	ap->dz *= ke;
	}
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
SKIP: ; /* if we are here from goto we have done two half steps (or more)*/
		
	}/* end of istep loop */
	return 1;
}
/* routine pacpac( nstep,dtime)
*int pacpac(forces,nforces, nstep,dtime)
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int pacpac(forces,nforces, nstep,dtime)
int (*forces[])(),nforces;
int nstep;
float dtime;
{
	ATOM *ap,*a_next(),*bp,*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces,icorrect;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->gx = ap->vx;	
		ap->gy = ap->vy;	
		ap->gz = ap->vz;	
		ap->vx += ap->fx/ap->mass*dtime*4.184e6;
		ap->vy += ap->fy/ap->mass*dtime*4.184e6;
		ap->vz += ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	if( ap->mass <= 0.) 
		{
		ap->gx = ap->vx;	
		ap->gy = ap->vy;	
		ap->gz = ap->vz;	
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		} /* end of mass check */
	}
/* make up the new prediction direction */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		ap->dx = ap->vx + ap->gx;
		ap->dy = ap->vy + ap->gy;
		ap->dz = ap->vz + ap->gz;
	}
	for( icorrect = 0;icorrect < 2; icorrect ++)
	{
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/4.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->vx = ap->gx + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->gy + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->gz + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		} /* end of mass check */
	}
/* make up the new prediction direction */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		ap->dx = ap->vx + ap->gx;
		ap->dy = ap->vy + ap->gy;
		ap->dz = ap->vz + ap->gz;
	}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		ap->x += .5*(ap->vx + ap->gx)*dtime;
		ap->y += .5*(ap->vy + ap->gy)*dtime;
		ap->z += .5*(ap->vz + ap->gz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine hpac( nstep,dtime)
*int hpac(forces,nforces, nstep,dtime,H)
*
* perform nstep pac dynamics with dtime
* kinetic energy adusted for constant H
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int hpac(forces,poten,nforces,nstep,dtime_real,H)
int (*forces[])(),(*poten[])(),nforces;
int nstep;
float dtime_real,H;
{
	ATOM *ap,*bp,*a_next(),*bonded[10];
	float ke,Tke;
	float alpha;
	float dtime;
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();


	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	ke = 0.;
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ke += ap->mass*(
	ap->vx*ap->vx + ap->vy*ap->vy + ap->vz*ap->vz);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}
	ke = ke*.5/4.184/1000/1000;  /* ke in kcal/mol */
/* get the current potential */
	Tke = 0.;
	for(i=0; i< nforces; i++)
		(*poten[i])(&Tke,0.);
/* scale the current velocities */
	dtime = dtime_real;
	if( Tke < H )
	{	
	ke = sqrt((H-Tke)/ke); /* ke is the scaled shift value */
	dtime = dtime_real/ke;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
                /* SPEC: fix # of arguments - jh/9/21/99 */
		hpac(forces,poten,nforces,1,dtime_real*0.5,H); 
		hpac(forces,poten,nforces,1,dtime_real*0.5,H); 
		goto SKIP;
			}
	ap = a_next(-1);
	bp =  ap;
	for( i=0; i< imax;  i++, ap = bp)
	{
	bp = a_next(1);
	ap->dx *= ke;
	ap->dy *= ke;
	ap->dz *= ke;
	}
	} else { 
	aaerror("Warning in Hpac, Potential energy higher than target\n");
	a_v_zero();
	a_d_zero();
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
SKIP: ; /* if goto here we've had too large a step and used half steps */
		
	}/* end of istep loop */
	return 1;
}
/* routine ppac( nstep,dtime)
*int ppac(forces,nforces, nstep,dtime,P)
*
* force the pressure to be constant
* use P = integral ( f . r )dV as 	
* the basis for a diffeomorphism
*   P => kP or Integral( kf.r)dV
*         to enforce pressure
*   r => r/k to enforce physical reality
*   may need to damp this.
*  
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int ppac(forces,nforces, nstep,dtime_real,P)
int (*forces[])(),nforces;
int nstep;
float dtime_real,P;
{
	ATOM *ap,*bp,*a_next(),*bonded[10];
	float p,Tp,R;
	float dtime,cx,cy,cz;
	float alpha;
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */

	imax = a_number();
	if( imax <= 0 )return 0;
	for( istep = 0.; istep< nstep; istep++)
	{

	cx = 0.; cy = 0.; cz = 0.;	
/*  move the velocity vector into the displacment slot */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	cx += ap->x;
	cy += ap->y;
	cz += ap->z;
	}
	cx /= imax;
	cy /= imax;
	cz /= imax;

/* calculate the pressure */

	p = 0.;
	Tp = 0.;
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		p += ap->vx*ap->vx*ap->mass;
		p += ap->vy*ap->vy*ap->mass;
		p += ap->vz*ap->vz*ap->mass;
		Tp += (ap->x-cx)*(ap->x-cx);
		Tp += (ap->y-cy)*(ap->y-cy);
		Tp += (ap->z-cz)*(ap->z-cz);
	}
	Tp = sqrt(Tp/imax);
	Tp = 4*PI/3*Tp*Tp*Tp;
  	p = p/imax/Tp*.5; /* now mks molar */
	printf("P %f p %f Tp %f\n",P,p,Tp);
/* moment shift
	p = sqrt( P/p);
	dtime = dtime_real/p;
*/
	dtime = dtime_real;
/* this is about the steepest volume correction which works !! 
  1. + .2/1.2 and 1 + .5/1.5 fail
*/
	p = (1.+.1*pow( p/P, 1./3.))/1.1;
	
/* temporary kludge to understand problem */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
/*
		ap->vx *= p;	
		ap->vy *= p;	
		ap->vz *= p;	
		ap->dx *= p;	
		ap->dy *= p;	
		ap->dz *= p;	
*/

		ap->x *= p;
		ap->y *= p;
		ap->z *= p;
	}
/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);

/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine ptpac( nstep,dtime)
*int ptpac(forces,nforces, nstep,dtime,P,T)
*
* force the pressure to be constant
*  
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int ptpac(forces,nforces, nstep,dtime_real,P,T)
int (*forces[])(),nforces;
int nstep;
float dtime_real,P,T;
{
	ATOM *ap,*bp,*a_next(),*bonded[10];
	float p,Tp,R;
	float Tk;
	float dtime,cx,cy,cz;
	float alpha;
	int inbond,iflag;
	int a_f_zero(),a_inc_v();
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */

	imax = a_number();
	if( imax <= 0 )return 0;
	for( istep = 0.; istep< nstep; istep++)
	{

	cx = 0.; cy = 0.; cz = 0.;	
/*  move the velocity vector into the displacment slot */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	cx += ap->x;
	cy += ap->y;
	cz += ap->z;
	}
	cx /= imax;
	cy /= imax;
	cz /= imax;

/* calculate the pressure */

	p = 0.;
	Tp = 0.;
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		p += ap->vx*ap->vx*ap->mass;
		p += ap->vy*ap->vy*ap->mass;
		p += ap->vz*ap->vz*ap->mass;
		Tp += (ap->x-cx)*(ap->x-cx);
		Tp += (ap->y-cy)*(ap->y-cy);
		Tp += (ap->z-cz)*(ap->z-cz);
	}
	Tp = sqrt(Tp/imax);
	Tp = 4*PI/3*Tp*Tp*Tp;
	Tk = 3*imax*R*4.184*1000;
	Tk = p/Tk;  /* Tk is now the temperature */
	if( Tk < 1.e-5) Tk = 1.;
  	p = p/imax/Tp*.5; /* now mks molar  ( kilopascal's because of grams)*/
	printf("P %f p %f Tp %f\n",P,p,Tp);
/* momentum shift */
	Tk = sqrt(T/Tk);
	dtime = dtime_real/Tk;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
		ptpac(forces,nforces,1,dtime_real*0.5,P,T); 
		ptpac(forces,nforces,1,dtime_real*0.5,P,T); 
		goto SKIP;
			}
/* this is about the steepest volume correction which works !! 
  1. + .2/1.2 and 1 + .5/1.5 fail
also checked that the current 'pressure' is the best to use
for stable running  
*/
	p = (1.+.1*pow( p/P, 1./3.))/1.1;
	
/* temporary kludge to understand problem */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		ap->vx *= Tk;	
		ap->vy *= Tk;	
		ap->vz *= Tk;	
		ap->dx *= Tk;	
		ap->dy *= Tk;	
		ap->dz *= Tk;	

		ap->x *= p;
		ap->y *= p;
		ap->z *= p;
	}
/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);

/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
SKIP: ; /* if goto here we've had too large a step and used half steps */
	}/* end of istep loop */
	return 1;
}
