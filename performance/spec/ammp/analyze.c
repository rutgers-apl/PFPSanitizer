/* analyze.c
*
*  routine to analyze energy and force for AMMP.
*
*  analyzes the potential due to each kind of potential used
*
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
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-bonded potentials
*/
static void start_slice(){
  __asm__ __volatile__ ("");
}

static void end_slice(){
  __asm__ __volatile__ ("");
}

void analyze( vfs,nfs,ilow,ihigh,op )
int  (*vfs[])();
int nfs;
FILE *op;
int ilow,ihigh;
{
  //start_slice();
  /* block of function used in eval()
   *   only the v_stuff are needed
   */
  int v_bond(),f_bond(),v_angle(),f_angle();
  int v_mmbond(),f_mmbond(),v_mmangle(),f_mmangle();
  int v_nonbon(),f_nonbon(),v_torsion(),f_torsion();
  int atom(),bond(),angle(),torsion();
  int v_hybrid(),f_hybrid();
  int restrain(),v_restrain(),f_restrain();
  int tether(),v_tether(),f_tether();
  int u_v_nonbon(), u_f_nonbon();
  int v_noel(),a_noel();
  int v_ho_noel();
  int a_bond(),a_mmbond(),a_angle(),a_mmangle();
  int a_nonbon(),a_torsion(),a_hybrid(),a_restrain();
  int a_tether();

  float V,T,vt;
  int ifs;
  int i,j;
  i = ilow;
  j = ihigh;
  if( ihigh < ilow ) j = ilow;
  V = 0.;
  for( ifs = 0; ifs < nfs; ifs++ )
  {
    vt = 0.; 
    /*	(*vfs[ifs])(&vt,0.); */
    if( vfs[ifs] == v_bond)
    { a_bond(&vt,0.,i,j,op);fprintf( op," %f bond energy\n",vt); }
    else if( vfs[ifs] == v_mmbond)
    {a_mmbond(&vt,0.,i,j,op); fprintf( op," %f mm bond energy\n",vt);}
    else if( vfs[ifs] == v_mmangle)
    {a_mmangle(&vt,0.,i,j,op); fprintf( op," %f mm angle energy\n",vt);}
    else if( vfs[ifs] == v_angle)
    {a_angle(&vt,0.,i,j,op); fprintf( op," %f angle energy\n",vt);}
    else if( vfs[ifs] == v_noel)
    {a_noel(&vt,0.,i,j,op); fprintf( op," %f noel energy\n",vt); }
    else if( vfs[ifs] == v_ho_noel)
    {a_noel(&vt,0.,i,j,op); fprintf( op," %f noel energy\n",vt); }
    else if( vfs[ifs] == u_v_nonbon)
    {a_nonbon(&vt,0.,i,j,op); fprintf( op," %f non-bonded energy\n",vt);}
    else if( vfs[ifs] == v_nonbon)
    {a_nonbon(&vt,0.,i,j,op); fprintf( op," %f non-bonded energy\n",vt); }
    else if( vfs[ifs] == v_torsion)
    {a_torsion(&vt,0.,i,j,op); fprintf( op," %f torsion energy\n",vt); }
    else if( vfs[ifs] == v_hybrid)
    {a_hybrid(&vt,0.,i,j,op); fprintf( op," %f hybrid energy\n",vt);}
    else if( vfs[ifs] == v_tether)
    {a_tether(&vt,0.,i,j,op); fprintf( op," %f tether restraint energy\n",vt);}
    else if( vfs[ifs] == v_restrain)
    {a_restrain(&vt,0.,i,j,op); fprintf( op," %f restraint bond energy\n",vt);}
    /* next statement is needed because cannot have a label at an end loop */

    V += vt;
    vt = 0.;
  }
  fprintf( op," %f total potential energy\n",V);
  /* end of routine */
  //end_slice();
}

