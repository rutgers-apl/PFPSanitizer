/* ammp.h
*  include file for ammp and routines associated with it
*
*  define the ATOM structure
*/
/*  excluded stuff is for a list of bonded and 1-3 atoms not to be
*   used in the non-bonded evaluation 
*   the purpose of this is to speed up the nonbonded calculations since
*   the bond list varies only once and a while
*/
/* dont use double precision */
/* do use it  - SPEC jh/9/30/99 */
#define float double

#define NEXCLUDE 32 

/*
#define QUARTIC
#define  QUINTIC 
#define CUBIC  
*/

#ifdef QUARTIC
#define CUBIC 
#endif

#ifdef QUINTIC
#define CUBIC 
#define QUARTIC
#endif

#define NCLOSE 200  

/* on the SGI use a different malloc 
* SPEC: no, I think this section is not used; comment it out and
* see what happens  - jh/9/30/99
* #ifdef SGI
* #include <sys/types.h>
* #include <malloc.h>
* #endif
* #ifdef DECALPHA
* #include <malloc.h>
* #endif
*/ 

#define critical_precision double

#ifdef ALLSINGLE
#define critical_precision float
#endif

#ifdef ALLDOUBLE
#define float double
#endif

typedef struct{
	float x,y,z;
	critical_precision fx,fy,fz;
	int serial;
	float q,a,b,mass;
	void *next;
	char active;
	char name[9];
	float chi,jaa;
	float vx,vy,vz,vw,dx,dy,dz;
	float gx,gy,gz;
	float VP,px,py,pz,dpx,dpy,dpz; 
/*      float dpxx,dpxy,dpxz,dpyy,dpyz,dpzz; */
/* place holders for interpolation on V */
	float qxx,qxy,qxz,qyy,qyz,qzz;
#ifdef CUBIC
	float qxxx,qxxy,qxxz,qxyy,qxyz,qxzz;
	float qyyy,qyyz,qyzz,qzzz;
#endif
#ifdef QUARTIC
	float qxxxx,qxxxy,qxxxz,qxxyy,qxxyz,qxxzz;
	float qxyyy,qxyyz,qxyzz,qxzzz;
	float qyyyy,qyyyz,qyyzz,qyzzz,qzzzz;
#endif
#ifdef QUINTIC
	float qxxxxx,qxxxxy,qxxxxz,qxxxyy,qxxxyz,qxxxzz;
	float qxxyyy,qxxyyz,qxxyzz,qxxzzz;
	float qxyyyy,qxyyyz,qxyyzz,qxyzzz,qxzzzz;
	float qyyyyy,qyyyyz,qyyyzz,qyyzzz,qyzzzz,qzzzzz;
#endif
/* interpolation on force */
	void *close[NCLOSE];
	void *excluded[NEXCLUDE];
	char exkind[NEXCLUDE];
/* bitmap way to do it
#if (NEXCLUDE <33 )
	unsigned exkind;
#else
	unsigned long exkind;
#endif
*/
	int  dontuse;
	} ATOM;



#include "numeric.h" 

/* SPEC need some function protos to avoid lots of warnings jh/9/21/99 
* In order to prevent more warnings, we do these in modern format,
* and also change the functions themselves to use new-style declaration
* instead of K&R style.   NOTE: In this pass through ammp, no attempt
* has been made to fix *every* function declaration.  Only the ones that
* led to compiler whinings.
*/
void aaerror ( char *);
//int  a_ftodx( float , float );
void a_inactive_f_zero();
void get_bond( ATOM *, ATOM *[], int , int * );
void mom_param( int serial, float chi, float jaa );
void rand3( float *, float *, float * );
int  set_f_variable ( char *, float );
int a_inc_d( float lambda );
float get_f_variable(char *name );
int a_f_zero();
int a_ftodx(float,float);
float a_max_f();
int a_ftogx( float lambda, float lamold);
int a_g_zero();
int a_d_zero();
//float a_pr_beta();
float a_max_d();
int a_f_zero();

/* SPEC Since the "dump" ones are used in several places, might as well 
* define those here too, but that means we need stdio here.
*/
#ifndef FILE
#include <stdio.h>
#endif
float randg();
void dump_angles( FILE * );
void dump_atoms( FILE * );
void dump_bonds( FILE * );
void dump_excludes( FILE * );
void dump_force( FILE * );
void dump_hybrids( FILE * );
void dump_noels( FILE * );
void dump_pdb( FILE *, int );
void dump_restrains( FILE * );
void dump_tethers( FILE * );
void dump_tgroup( FILE * );
void dump_torsions( FILE * );
void dump_variable( FILE * );
void dump_velocity( FILE * );
