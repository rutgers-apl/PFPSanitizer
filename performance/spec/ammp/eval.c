/*
*  Stripped down version of AMMP for benchmarking
*
*  AMMP is stripped down to a minimal core of useful routines
*  to save time for the optimizers
*
*  bonds,angles,torsions,restrains,noels,hybrids kept
*  only point charge vdw
*  4d embedding removed
*  all molec. dynamics kept
*  cngdel,steep,gsdg kept
* mom kept
*  tailor kept
*  analyze kept
*
*  added box routine 
*  this will allow the use of fmm on big systems
*
*  benchmark only
*  added
*  signify ;  check the value of drift (setf delta)
*             check the value of rmsd (use tethers)
*             check that the ns value is set
*
*/
/* ammp.c
* Another Molecular Mechanics Program
*
*  this essentially runs the intermediate code for 
*  a molecular mechanics program
*
* instructions are of the form
*  ident <parameters> ;
*  # <stuff> ; is a comment
*   "<stuff>" is  a literal string
*  most instructions can be nested, but NOT loop<if> and labels 
*
*  allowed idents
*
*	atom   - atom record
*	bond   - bond record
*       morse  - morse record
*	angle  - angle record
* 	torsion - torsion record
*       hybrid  - hybrid (pyramid height) record
*	abc    - angle bond correlation record 
*                  i1 i2 i3 angle zero_angle  dr/da dk12/da dk23/da
*	av5  - tetrahedral 'volume' or centroid
*		i1,i2,i3,i4,i5, k, value
*	noel -noe distance restraint
*       velocity  - velocity record
*	read <file>  open and read from file untill done
*	output <file> <vers>  open and use for output file
*	dump <atom,bond,angle,abc,torsion,hybrid,av5,morse,pdb,variable,velocity,force> 
*                         write out the results
*	analyze ilow,ihigh  write out the errors in the current potential for atoms 
*				ilow to ihigh. if ilow > ihigh ilow to ilow
*	close  		close the current output file if not stdout
*	steep  niter,toler   steepest descents
*	bfgs  niter,toler  bfgs quasi newton 
*	cngdel  niter,ncut,toler  conjugate del  
*	trust   niter,dtoler,toler   trust optimizer
*       polytope imin,imax,niter,vstart,vfinal  polytope of a range
*	rigid imin,imax,niter, vstart,vfinal  polytope rigid body solver
*	echo <off>   echo to the user (turn off when dumping !!)
*       use  < none,bond,angle,abc,torsion,nonbon,morse,restrain,tether
*		,periodic,mmbond,mmangle,cangle,gauss,screen,debye,shadow,fourd
*		hobond hoangle trace honoel hotether image >  
*               flag on potentials
*       restrain    - restrain a distance 
*	tether      - tether an atom to a positon
*          tether serial fk x y z
*          tether all fk x y z  do all of them
*	tailor  qab   number q a b  - set the qab parameters of an atom
*       tailor  exclude  number number  - add an interaction to the nonbon exclude list
*       tailor  include number number  - delete an interaction from the nonbon exclude list
*	setf name value  set a float into the variable store
*       seti name value   set an int into the variable store
*       loopi label init max delta  loop to label while init < max integer vers.
*       loopf label init max delta  loop to label while init < max float vers.
*       label:    
*	monitor    find potential energy and kinetic energy, and calculate the forces
*	mon2    find potential energy and kinetic energy, and calculate the forces
*                 but only report the total energy
*       v_maxwell  temperature,dx,dy,dz
*	v_rescale   temperature
*       verlet       nstep,dtime (dtime is in m/s = .01A/ps)
*       pac          nstep,dtime (dtime is in m/s = .01A/ps)
*       tpac          nstep,dtime,Temp (dtime in m/s = .01A/ps,1fs = .00001)
*       ppac          nstep,dtime,pressure (dtime in m/s = .01A/ps,1fs = .00001)
*       ptpac          nstep,dtime,pressure,Temp (dtime in m/s = .01A/ps,1fs = .00001)
*       hpac          nstep,dtime,Htarget (dtime in m/s = .01A/ps,1fs = .00001)
*       pacpac       nstep,dtime (dtime is in m/s = .01A/ps)
*	doubletime   nstep,dlong,dshort,temper  double time scale dynamics
*	dipole first,last  calculate the dipole moment for atoms first to last
*                          assumes sequential atom numbers...
*	tgroup id serial1 serial2 serial3 serial4 base number
*            define a tgroup( torsion by serial numbers) base = zeropoint
*	     number == number of steps.  The group of atoms is everything bonded to 
*	      serial3 that isn't serial 2.
*	tsearch id id id id (up to 8  - terminated by 0 or ; ) 
*             search the tgroups defined
*
*       tset i1 i2 i3 i4 where
*            set the torsion angle defined by i1...i4 to where
*            unlike tgroup,tsearch only one angle at a time, and
*            no limit to the number of atoms rotated
*	tmin i1 i2 i3 i4 nstep
*            search the torsion angle  i1...i4 for the minimum
*            energy in nsteps
*	tmap i1 i2 i3 i4 j1 j2 j3 j4 ni nj;  map in ni nj steps
*              the i j atoms over all 360 degrees;
* 
* 	mompar  serial,chi,jaa  add electronegativity and self colomb to atom serial
*	momadd  serial serial  adds atoms to the MOM stack( can just be called with one)
*       mom   tq, niter   solves current mom stack for charges  
*			tq = total charge, niter = number of iterations (20 default)
*
*       time  return time of day and elapsed time (not on all machines)
*
*	math routines  see math.c
*		add a b ;
*		sub a b ;
*		mul a b;
*		div a b;
*		nop a;  these routines can work with atomic parameters 
*		mov a b;  variables, and imeadiate values.
*		max a b;
*		min a b;
*		randf a ;
*
*	  serial a i atomid;  put the serial number or residue i, atom atomid
*                   into a
*	index a i;  put the serial number of the ith atom into a;
*
*        je a b label: ;   jump a == b
*        jl a b label: ;   jump a < b
*        jg a b label: ;   jump a > b
*	jes a string label: ; dump to label if a->name == string
*	jnes a string label: ; dump to label if a->name != string
*           jumps are restricted to the current file
*
*	exit         - exit the routine - in case EOF is not defined
*
*  	active i1 i2; <i2 optional> active atoms i1 to i2 (default is active)
*       inactive i1 i2; < i2 optional> inactivate atoms i1 to i2 
*       nzinactive i1 i2; < i2 optional> inactivate atoms i1 to i2 that
*                               are not 0 0 0  
*
*
* 	grasp nstep nopt imin imax atom;  GRASP in torsion space
*	genetic nstep ndeep sigma target n_opt_steps ; genetic optimizer
*	gsdg  niter min_atom max_atom; iterative distance geometry bounded by
*                                       serial numbers
*	bell  niter min_atom max_atom; iterative distance geometry bounded by
*                                       serial numbers
*
*	dgeom niter origin shift;  standard distance geometry
*                             implemented with the power method
*                              origin is the atom to use as the key
*                              shift is the amount of eigenvalue shift
*
*	normal damp    ;    calculate the normal modes  if damp > 0 output them
*
*	table id n ; create empty sorted table
*       tableent id who r v ; add the who'th element to the table it
*       access with use tbond
*
* direct SCF terms
*       orbit <o1,o1o,o2,o3,o4s,o4p,om> i1,<i2-i5>,osn, parameters, ipair ;
*               ipair == 2 (doublet) ipair == 1 (singlet)
*       expand osn,n,a,r,a,r (up to 6)  ;
*                          these define an orbital
*
*       dscf <coef,expo,xyz,geom,anal> n toler;  optimize the orbitals
*            <coefficients, exponents, atom center, orbital geometry>;
*
*	others like fix,and... TBD
*   first nonblank == '#' is a comment and the line is skipped 
*/
/*
*  copyright 1992,1993 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/
#define ANSI 1
#define MAXTOKEN 20 
#define TOKENLENGTH 80 
/* misc includes - ANSI and some are just to be safe */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>
#ifdef ANSI
#include <stdlib.h>
#endif
#ifdef ESV
#define NOTIME
#endif
#ifndef NOTIME
#define TIME
#endif

#include "ammp.h"
#ifdef GRACELESS
#include "graceless.h"
#endif

/* SPEC prevent compiler warnings by adding function proto jh/9/21/99 */
int loadloop (FILE *, FILE *, char *);

void read_eval_do( ip,op )
FILE *ip,*op;
{
char line[4096], *ap, *ap1; /* buffer and pointers for sscanf */
int  itemp[MAXTOKEN];  /* integers to read */
float ftemp[MAXTOKEN]; /* floats to read */
int inbuff;     /* where am i in the buffer */
int inliteral ; /* inside of a literal string */
int eval();
/* always start outside of a literal */

	inliteral = (1 == 0); /* portable lexpresion */
/* error checking */
	if( ip == NULL)
	{
		aaerror(" cannot use input file \n");
		return ;
	}
	if( op == NULL)
	{
		aaerror(" cannot use output file \n");
		return;
	}
/* lex out a line  (i.e.  <stuff> ; )
* filter out \n and the \; */
	inbuff = 0;
	while((line[inbuff]=fgetc(ip))!= (char)EOF)
	{/* start of the lex while */
	if( !inliteral && line[inbuff] == '"') inliteral = (1==1);
	if(  inliteral && line[inbuff] == '"') inliteral = (1==0);
	if( !inliteral ){
	if( line[inbuff] == ',') line[inbuff] = ' ';
	if( line[inbuff] == '\t') line[inbuff] = ' ';
	if( line[inbuff] == '\n') line[inbuff] = ' ';
	if( line[inbuff] == ';')
	{
	line[inbuff] = '\0';
	if(eval(ip,op,line )<0) return;
	inbuff = 0;
	} else if( line[inbuff] != '\n') inbuff++;
		}/* end of inliteral */
	 }/* end of the lex while */
}/* end of routine */

/* eval actually parses the line */
/* original version used sscanf *
*  current version lexes tokens and if numeric
*  converts them to integer and floating point versions
*/ 
  int  (*potentials[10])(),(*forces[10])(),nused=(-1);
int eval( ip,op,line )
FILE *ip,*op;
char *line;
{
FILE *newfile,*fopen(),*tmpfile();
char  token[MAXTOKEN][TOKENLENGTH],*ap, *ap1; 
	/* buffer and pointers for sscanf */
char   errmes[80];
int  itemp[MAXTOKEN],itoken,tisvariable(),tisint();  /* integers to read */
float ftemp[MAXTOKEN]; /* floats to read */
/*static  int  (*potentials[10])(),(*forces[10])(),nused=(-1); */
static  int  echo=1;  
static int inloop = 1;

/* SPEC we now declare set_f_variable in ammp.h jh/9/21/99 */
/* int get_i_variable(),set_i_variable(),set_f_variable(); */
int get_i_variable(),set_i_variable();

float get_f_variable();
int v_bond(),f_bond(),v_angle(),f_angle();
int v_mmbond(),f_mmbond(),v_mmangle(),f_mmangle();
int v_c_angle(), f_c_angle();
int v_periodic(),f_periodic();
int v_nonbon(),f_nonbon(),v_torsion(),f_torsion();
int v_box(),f_box();
int u_v_nonbon(),u_f_nonbon();
int v_ho_bond(),f_ho_bond();
int v_ho_angle(),f_ho_angle();
int atom(),bond(),angle(),torsion(),a_readvelocity();
int restrain(),v_restrain(),f_restrain();
int tether(),v_tether(),f_tether(),alltether();
int v_ho_tether(),f_ho_tether();
int hybrid(),v_hybrid(),f_hybrid();
int noel(),v_noel(),f_noel();
int v_ho_noel(),f_ho_noel();
int math();
#ifdef TIME
clock_t clock();
#endif
int significance();
void gsdg();
void analyze();
void AMMPmonitor();
void AMMPmonitor_mute();
void mom_add(),mom();
void tailor_qab();
void tailor_include();
void tailor_exclude();
int verlet(),v_maxwell(),v_rescale();
int pac(),pacpac(),tpac(),hpac(),ppac();
int ptpac();
/* default setup of potentials and forces */
if( nused == -1) {
 potentials[0] = v_bond;
potentials[1] = v_angle;
potentials[2] = u_v_nonbon;
potentials[3] = v_torsion;
potentials[4] = v_hybrid;
forces[0] = f_bond;
forces[1] = f_angle;
forces[2] = u_f_nonbon;
forces[3] = f_torsion;
forces[4] = f_hybrid;
nused = 5;
}
/* for safety and to avoid side effects the token arrays are zero'd */
 	for( itoken=0; itoken<MAXTOKEN; itoken++)
	{
		token[itoken][0] = '\0';
		itemp[itoken] = 0; ftemp[itoken] = 0.;
	}
/* now extract tokens and prepare to match it */

	if( echo ) fprintf(op,"%s;\n",line);
	ap = line;
	for( itoken=0; itoken< MAXTOKEN; itoken++)
	{	
	ap1 = &token[itoken][0];
	*ap1 = '\0';
	while(*ap == ' ') ap++;
	if( *ap == '"') { /* its a literal copy until '"' is seen */
		ap++;
		while( *ap != '"' && *ap != '\0')
		{
			*(ap1++) = *(ap++);
		}
		if( *ap == '"' ) ap++;
			}else {/* not literal */
	if( itoken== 0 && *ap == '#') return 1;
	while(*ap != ' ' && *ap != '\0')
	{
		if( itoken == 0 ||  ( strcmp(&token[0][0],"read") != 0 &&
				    strcmp(&token[0][0],"output") != 0)  )
		{
		if( isupper(*ap))
		{*ap1 = tolower(*ap); }else{ *ap1 = *ap;}
		ap1++; ap++;
		} else {
		*ap1 = *ap; ap1++; ap++; 
		} 
	}
			}/* end of if not literal */
	*ap1 = '\0';     
/*  now have a list of lexed tokens */
/*	printf(" %d %s \n",itoken,&token[itoken][0]);  */ 
/* if the token is a number atof or atoi it */
	ap1 = &token[itoken][0];
	if( tisvariable(ap1) )
	{
/* printf(" %s is a variable\n",ap1);  */
/*  here is where the variable fetch command will go */
		ftemp[itoken] = get_f_variable( ap1);
		itemp[itoken] = get_i_variable( ap1);
		} else{
		if( tisint(ap1) == 1  )
		{
/*		printf(" %s is an integer\n",ap1); */  
		itemp[itoken] = atoi(ap1);	
		ftemp[itoken] = itemp[itoken];
		}else{
/*		printf(" %s is a float\n",ap1);   */
		 ftemp[itoken] = atof(ap1);	
/*		sscanf( ap1,"%g",&ftemp[itoken]); */
		itemp[itoken] = (int)ftemp[itoken];
		}
	}
	if(*ap == '\0') break;
	}
	if( token[0][0]  == '\0') return 1 ;  
	/* blank lines are not an error */

/* the block ifs are used rather than a switch to manage
* potential complexity of  man commands 
* each if done in the general pattern will have no  side
* effects and is therfor well phrased.
* needless to say common commands should be first 
*/ 
	if( strcmp( &token[0][0], "atom" ) == 0 )
	{
	if(atom( ftemp[1],ftemp[2],ftemp[3],itemp[4],ftemp[6],
		ftemp[7],ftemp[8],ftemp[9],&token[5][0]) )
	{ } else { 
	aaerror(" cannot add to atom structure -data structure error");
			exit(0); }
	goto DONE;
	}

	if( strcmp( &token[0][0], "bond" ) == 0 )
	{
	if( bond(itemp[1],itemp[2],ftemp[3],ftemp[4]))
	{ }else
	{
	 aaerror(" cannot add to bond structure -data structure error"); 
		exit(0); }
	goto DONE;
	}

	if( strcmp( &token[0][0], "restrain" ) == 0 )
	{
	if( restrain(itemp[1],itemp[2],ftemp[3],ftemp[4]))
	{ }else
	{ aaerror(" cannot add to restrain structure -data structure error"); 
	exit(0);}
	goto DONE;
	}
	if( strcmp( &token[0][0], "angle" ) == 0 )
	{
	ftemp[2] = 3.141592653589793/180.;
	ftemp[5] = ftemp[5]*ftemp[2];
	if( angle( itemp[1],itemp[2],itemp[3],ftemp[4],ftemp[5]) )
	{ } else
	{ aaerror(" cannot add to angle structure -data structure error"); 
	exit(0);}
	goto DONE;
	}
	if( strcmp( &token[0][0], "noel" ) == 0 )
	{
	if( noel(itemp[1],itemp[2],ftemp[3],ftemp[4],ftemp[5],
			ftemp[6],ftemp[7]))
	{ }else
	{
	 aaerror(" cannot add to noel structure -data structure error"); 
		exit(0); }
	goto DONE;
	}


	if( strcmp( &token[0][0], "torsion" ) == 0 )
	{
	ftemp[2] = acos(-1.)/180.; ftemp[7] = ftemp[7]*ftemp[2];
	if( torsion(itemp[1],itemp[2],itemp[3],itemp[4],ftemp[5],itemp[6],
			ftemp[7]) )
	{ } else
	{ aaerror(" cannot add to torsion structure -data structure error"); 
	exit(0);}
	goto DONE;
	}
	if( strcmp( &token[0][0], "hybrid" ) == 0 )
	{
	if(hybrid(itemp[1],itemp[2],itemp[3],itemp[4],ftemp[5],ftemp[6]))
	{ } else
	{ aaerror(" cannot add to hybrid structure -data structure error"); 
	exit(0);}
	goto DONE;
	}





	if( strcmp( &token[0][0], "velocity" ) == 0 )
	{
	if( a_readvelocity(itemp[1],ftemp[2],ftemp[3],ftemp[4]) )
	{ } else
	{ aaerror(" cannot update velocity -is this atom defined? "); 
	exit(0);}
	goto DONE;
	}
	if( strcmp( &token[0][0], "tether" ) == 0 )
	{
	if( strcmp( &token[1][0], "all") == 0 )
	{
		if( alltether( ftemp[2] ) )
		{} else
		{ aaerror(" cannot add to tether structure -data structure error"); 
		exit(0);}
	}else
	{
		if( tether( itemp[1],ftemp[2],ftemp[3],ftemp[4],ftemp[5]) )
		{ } else
		{ aaerror(" cannot add to tether structure -data structure error"); 
	exit(0);}
	}
	goto DONE;
	}

	if( strcmp( &token[0][0],"tgroup") == 0)
	{
	tgroup( itemp[1],itemp[2],itemp[3],itemp[4],itemp[5],ftemp[6],itemp[7]);
	goto DONE ;
	} 

	if( strcmp( &token[0][0],"tsearch") == 0)
	{
	tsearch( itemp[1],itemp[2],itemp[3],itemp[4],itemp[5],itemp[6],itemp[7],itemp[8]);
	goto DONE ;
	} 
	if( strcmp( &token[0][0],"tset") == 0 )
	{
	tset( op,echo,itemp[1],itemp[2],itemp[3],itemp[4],ftemp[5]*3.141592653589793/180.);
	goto DONE ;
	}
	if( strcmp( &token[0][0],"tmin") == 0 )
	{
	tmin( op,echo,itemp[1],itemp[2],itemp[3],itemp[4],itemp[5],potentials,nused);
	goto DONE ;
	}
	if( strcmp( &token[0][0],"tmap") == 0 )
	{
	tmap( op,echo,potentials,nused,
	itemp[1],itemp[2],itemp[3],itemp[4],
	itemp[5],itemp[6],itemp[7],itemp[8],itemp[9],itemp[10]
	);
	goto DONE ;
	}

	if( strcmp( &token[0][0], "mompar" )== 0)
	{
	mom_param( itemp[1],ftemp[2],ftemp[3] );
	goto DONE;
	}
	if( strcmp( &token[0][0], "momadd" )== 0)
	{
	mom_add( itemp[1],itemp[2] );
	goto DONE;
	}
	if( strcmp( &token[0][0], "mom" )== 0)
	{
	mom( op,ftemp[1],itemp[2] );
	goto DONE;
	}
	if( strcmp( &token[0][0], "monitor" )== 0)
	{
	AMMPmonitor( potentials,forces,nused,op );
	goto DONE;
	}
	if( strcmp( &token[0][0], "mon2" )== 0)
	{
	AMMPmonitor_mute( potentials,forces,nused,op );
	goto DONE;
	}
	if( strcmp( &token[0][0], "nzinactive" ) == 0)
	{
	inactivate_non_zero( itemp[1],itemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "inactive" ) == 0)
	{
	inactivate( itemp[1],itemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "active" ) == 0)
	{
	activate( itemp[1],itemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0],"signify") ==0 )
	{
	significance(op);
	goto DONE;
	}
	if( strcmp( &token[0][0], "analyze" )== 0)
	{
	analyze( potentials,nused,itemp[1],itemp[2],op );
	goto DONE;
	}
	if( strcmp(&token[0][0] , "tailor" ) == 0)
	{
		if( strcmp(&token[1][0], "qab") == 0 )
		{
		tailor_qab( itemp[2], ftemp[3],ftemp[4],ftemp[5]);
		goto DONE;
		}
		if( strcmp(&token[1][0], "include") == 0 )
		{
		tailor_include( itemp[2],itemp[3]);
		goto DONE;
		}
		if( strcmp(&token[1][0], "exclude") == 0 )
		{
		tailor_exclude( itemp[2],itemp[3]);
		goto DONE;
		}
	aaerror(" undefined tailor option "); aaerror(&token[1][0]); 
	goto DONE;
	}

	if( strcmp( &token[0][0], "read" ) == 0 )
	{
	newfile = fopen( &token[1][0],"r");
	if( newfile == NULL )
	{ aaerror(" cannot open file for read "); aaerror(&token[1][0]); }
	 else
	{ read_eval_do(newfile,op); fclose(newfile); }
	goto DONE;
	}
	if( strcmp( &token[0][0], "output" ) == 0 )
	{
/* if a non-zero version then write it out */
	if( itemp[2] > 0) 
	{
	sprintf( errmes,"%s.%d",&token[1][0],itemp[2]);
	newfile = fopen( errmes,"w");
	} else {
	newfile = fopen( &token[1][0],"w");
	}
	if( newfile == NULL )
	{ aaerror(" cannot open file for write "); aaerror(&token[1][0]); } 
	else
	{ read_eval_do(ip,newfile); }
	goto DONE;
	}
	if( strcmp( &token[0][0], "dump" ) == 0 )
	{
	for( itoken=1; itoken<MAXTOKEN; itoken++)
	{if( token[itoken][0] == '\0') goto DONE;
	if( strcmp(&token[itoken][0],"atom") == 0) dump_atoms( op);
	if( strcmp(&token[itoken][0],"bond") == 0) dump_bonds( op);
	if( strcmp(&token[itoken][0],"noel") == 0) dump_noels( op);
	if( strcmp(&token[itoken][0],"angle") == 0) dump_angles( op);
	if( strcmp(&token[itoken][0],"torsion") == 0) dump_torsions( op);
	if( strcmp(&token[itoken][0],"hybrid") == 0) dump_hybrids( op);
	if( strcmp(&token[itoken][0],"restrain") == 0) dump_restrains( op);
	if( strcmp(&token[itoken][0],"pdb") == 0) dump_pdb( op,100);
	if( strcmp(&token[itoken][0],"variable") == 0) dump_variable(op);
	if( strcmp(&token[itoken][0],"velocity") == 0) dump_velocity(op);
	if( strcmp(&token[itoken][0],"force") == 0) dump_force(op);
	if( strcmp(&token[itoken][0],"tether") == 0) dump_tethers(op);
	if( strcmp(&token[itoken][0],"tgroup") == 0) dump_tgroup(op);
	}
	goto DONE;
	}
	if( strcmp( &token[0][0], "use" ) == 0 )
	{
	for( itoken=1; itoken<MAXTOKEN; itoken++)
	{
	if( token[itoken][0] == '\0') goto DONE;
	if( strcmp(&token[itoken][0],"none") == 0) nused = 0;
	if( strcmp(&token[itoken][0],"nonbon") == 0)
	{forces[nused] = u_f_nonbon; potentials[nused++] = u_v_nonbon;} 
	if( strcmp(&token[itoken][0],"bond") == 0) 
	{forces[nused] = f_bond; potentials[nused++] = v_bond;} 
	if( strcmp(&token[itoken][0],"mmbond") == 0) 
	{forces[nused] = f_mmbond; potentials[nused++] = v_mmbond;} 
	if( strcmp(&token[itoken][0],"hobond") == 0) 
	{forces[nused] = f_ho_bond; potentials[nused++] = v_ho_bond;} 
	if( strcmp(&token[itoken][0],"tether") == 0) 
	{forces[nused] = f_tether; potentials[nused++] = v_tether;} 
	if( strcmp(&token[itoken][0],"hotether") == 0) 
	{forces[nused] = f_ho_tether; potentials[nused++] = v_ho_tether;} 
	if( strcmp(&token[itoken][0],"restrain") == 0) 
	{forces[nused] = f_restrain; potentials[nused++] = v_restrain;} 
	if( strcmp(&token[itoken][0],"angle") == 0) 
	{forces[nused] = f_angle; potentials[nused++] = v_angle;} 
	if( strcmp(&token[itoken][0],"hoangle") == 0) 
	{forces[nused] = f_ho_angle; potentials[nused++] = v_ho_angle;} 
	if( strcmp(&token[itoken][0],"mmangle") == 0) 
	{forces[nused] = f_mmangle; potentials[nused++] = v_mmangle;} 
	if( strcmp(&token[itoken][0],"cangle") == 0) 
	{forces[nused] = f_c_angle; potentials[nused++] = v_c_angle;} 
	if( strcmp(&token[itoken][0],"torsion") == 0) 
	{forces[nused] = f_torsion; potentials[nused++] = v_torsion;} 
	if( strcmp(&token[itoken][0],"hybrid") == 0) 
	{forces[nused] = f_hybrid; potentials[nused++] = v_hybrid;} 
	if( strcmp(&token[itoken][0],"honoel") == 0) 
	{forces[nused] = f_ho_noel; potentials[nused++] = v_ho_noel;} 
	if( strcmp(&token[itoken][0],"noel") == 0) 
	{forces[nused] = f_noel; potentials[nused++] = v_noel;} 
	if( strcmp(&token[itoken][0],"box") == 0) 
	{forces[nused] = f_box; potentials[nused++] = v_box;} 
	}
	goto DONE;
	}
	if( strcmp( &token[0][0], "close" ) == 0 )
	{
		if( op != stdout )
		{
		fclose(op);
		return -1;
		}   goto DONE;
	}
	if( strcmp( &token[0][0], "seti" ) == 0 )
	{
	if( token[1][0] == '\0') 
		{aaerror("seti requires a variable name: seti <name> value");
		goto DONE;
		}
	set_i_variable( &token[1][0], itemp[2]);
	goto DONE;	
	}
	if( strcmp( &token[0][0], "setf" ) == 0 )
	{
	if( token[1][0] == '\0') 
		{aaerror("setf requires a variable name: setf <name> value");
		goto DONE;
		}
	set_f_variable( &token[1][0], ftemp[2]);
	goto DONE;	
	}
	if( math( token,ftemp,itemp,ip,op,echo ) > 0 ) goto DONE;

	if( strcmp( &token[0][0], "v_maxwell") == 0)
	{
	v_maxwell( ftemp[1],ftemp[2],ftemp[3],ftemp[4]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "v_rescale") == 0)
	{
	v_rescale( ftemp[1]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "verlet") == 0)
	{
	verlet( forces,nused, itemp[1],ftemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "pac") == 0)
	{
	pac( forces,nused, itemp[1],ftemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "tpac") == 0)
	{
	tpac( forces,nused, itemp[1],ftemp[2],ftemp[3]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "ppac") == 0)
	{
	ppac( forces,nused, itemp[1],ftemp[2],ftemp[3]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "ptpac") == 0)
	{
	ptpac( forces,nused, itemp[1],ftemp[2],ftemp[3],ftemp[4]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "hpac") == 0)
	{
	hpac( forces,potentials,nused, itemp[1],ftemp[2],ftemp[3]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "pacpac") == 0)
	{
	pacpac( forces,nused, itemp[1],ftemp[2]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "steep" ) == 0 )
	{
	if( nused <= 0) goto DONE;
	steep( potentials,forces,nused,itemp[1],ftemp[2]);
	goto DONE;
	}

	if( strcmp( &token[0][0], "gsdg" ) == 0)
	{
	if( nused <= 0) goto DONE;
	gsdg( potentials,nused,itemp[1],itemp[2],itemp[3]);
	goto DONE;
	}
	if( strcmp( &token[0][0], "cngdel" ) == 0 )
	{
	if( nused <= 0) goto DONE;
	cngdel( potentials,forces,nused,itemp[1],itemp[2],ftemp[3],echo);
	goto DONE;
	}
#ifdef TIME 
	if( strcmp( &token[0][0],"time") == 0)
	{
	fprintf( op," %f CPU \n",((float)clock())/CLOCKS_PER_SEC); 
	goto DONE;
	}
#endif
	if( strcmp( &token[0][0],"echo" ) == 0)
	{
	echo = 1;
	if( strcmp( &token[1][0],"off") == 0) echo = 0;
	goto DONE;
	}
	if( strcmp( &token[0][0],"exit") ==0 ) return 1; //exit(0);
/* looping stuff */
	if( strcmp( &token[0][0],"loopi")  == 0)
	{
	if( token[1][0] == '\0') 
	{ aaerror(" must have a label to loop to "); goto DONE;}
	if( itemp[4] == 0) itemp[4] = 1;  /* must loop  */
	newfile = tmpfile();	
	if( newfile == NULL )
	{ aaerror(" cannot open temporary file in loopi"); goto DONE; }
/* scan the input data until the label is found */ 
	loadloop( ip,newfile, &token[1][0]);
/*  now do the loop */
	if( itemp[4] > 0)
	{
	for( itemp[0] = itemp[2];itemp[0]< itemp[3];itemp[0]+=itemp[4])
	{
	inloop = -1;
	if( tisvariable(&token[2][0])) 
	set_i_variable( &token[2][0], itemp[0]);
	rewind( newfile );
	 read_eval_do(newfile,op); 
	}
	} else{
	for( itemp[0] = itemp[2];itemp[0]< itemp[3];itemp[0]+=itemp[4])
	{
	inloop = -1;
	if( tisvariable(&token[2][0])) 
	set_i_variable( &token[2][0], itemp[0]);
	rewind( newfile );
	 read_eval_do(newfile,op); 
	}
	}
	inloop = 1;
	fclose( newfile);
	goto DONE;
	}
	if( strcmp( &token[0][0],"loopf")  == 0)
	{
	if( token[1][0] == '\0') 
	{ aaerror(" must have a label to loop to "); goto DONE;}
	if( ftemp[4] == 0.) ftemp[4] = 1.;  /* must loop  */
	newfile = tmpfile();	
	if( newfile == NULL )
	{ aaerror(" cannot open temporary file in loopi"); goto DONE; }
/* scan the input data until the label is found */ 
	loadloop( ip,newfile, &token[1][0]);
/*  now do the loop */
	if( ftemp[4] > 0.)
	{
	for( ftemp[0] = ftemp[2];ftemp[0]< ftemp[3];ftemp[0]+=ftemp[4])
	{
	inloop = -1;
	if( tisvariable(&token[2][0])) 
	set_f_variable( &token[2][0], ftemp[0]);
	rewind( newfile );
	 read_eval_do(newfile,op); 
	}
	} else  {
	for( ftemp[0] = ftemp[2];ftemp[0]> ftemp[3];ftemp[0]+=ftemp[4])
	{
	inloop = -1;
	if( tisvariable(&token[2][0])) 
	set_f_variable( &token[2][0], ftemp[0]);
	rewind( newfile );
	 read_eval_do(newfile,op); 
	}
	}
	inloop = 1;
	goto DONE;
	}
/* check if its a label  and return */
/* inloop returns -1 if in a loop which causes read_eval_do() to 
*  return and activates the loop routine */
	for( itemp[0]=0; itemp[0] < TOKENLENGTH; itemp[0]++)
	{
	if( token[0][itemp[0]] == '\0' || token[0][itemp[0]] == ' ')
	{
	 if( itemp[0] == 0) break;
	 if( token[0][itemp[0]-1] == ':') return inloop;
	}
	}
/*  default unrecognized token */
	sprintf(&errmes[0]," unrecognized token >%s<",&token[0][0]);
	aaerror( errmes );
DONE:
	return 1;
}
/* aaerror is a general error call function */
/* SPEC make this a void, since no one seems to use it jh/9/21/99 */
void aaerror( char *line )
{
	fprintf(stderr ,"%s \n",line);
	return ;
}
/* function tisvariable( char *p )
*
* returns 1 if the character string contains anything other than
* <+-><0-9>.<0-9><e><+-><0-9> 
* works on a tolowered string !!
*/
int tisvariable( p )
	char *p;
{
	if( (*p != '+')&&(*p != '-')&& !(isdigit( (int) *p)) &&(*p != '.') )
	 return 1;
/* now for the rest we check until either '\0' or not a digit */
	p++;
	while( (*p != '\0') && (isdigit( (int) *p) ) ) p++;
	if( *p == '\0') return 0;
	if( (*p != '.') && (*p != 'e') ) return 1;
	p++;
	if( !(isdigit( (int) *p)) ){
	if( *p == '\0' ) return 0;
	if( (*p != '.') && (*p != 'e') ) return 1;
	p++;
		}
	if( *p == '\0') return 0;
	if( (*p != '+')&&(*p != '-')&& !(isdigit( (int) *p)) &&(*p != '.') )
	 return 1;
	p++;
	if( *p == '\0') return 0;
	while( (*p != '\0') && ((isdigit( (int) *p))||(*p=='.')) ) p++;
	if( *p == '\0') return 0;
	return 1;
}
/* function tisint( char *p )
*
* check that a string is <+-><0-9>
* return 1 if true
* return 0 if not
*/
int tisint( p)
 char *p ;
{
	char *pp;
	pp = p;
	while( *pp != '\0') 
	{ if( *pp == '.') return 0; pp++;}
	if( (*p != '+')&&(*p != '-')&& !(isdigit( (int) *p)) ) return 0;
	p++;
	while (*p != '\0')
	{
		if( !(isdigit( (int) *p )) ) return 0;
		p++;
	}
	return 1;
}

/* routine loadloop( FILE *ip, FILE *tp, char *label)
*
* read lines from ip and write to tp
* when the line begins with label  stop (after writing it )
*/
/* SPEC use modern-style arg declaration, to match proto jh/9/22/99 */
int loadloop( FILE *ip, FILE *tp,  char *label)
{
	char line[256], *fgets() ;
	char *sp,*wp;
	
/*	printf( " the target label >%s<\n" , label);
*/
	while( fgets(  line,256,ip) != NULL )
	{
	fputs( line,tp );
	fputs("\n",tp);
	sp = line;
	while( *sp == ' ' && *sp != '\0') sp++;
	if( *sp != '\0' )
		{
		wp = sp;
		while(*wp != ';' && *wp != ' ' && *wp != '\0')
			{ if( isupper(*wp)){*wp = (char)tolower((int)*wp);} 
			 wp++;}
	if( *wp == ' ' ) *wp = '\0'; 
	if( *wp == ';' ) *wp = '\0'; 
        /* SPEC - add a "1" here to remove warnings, but I don't think */
        /*        any callers actually look at this return value       */
        /*        - j.henning 21-sep-99                                */
		if( strcmp(sp,label) == 0 ) return 1; 
		}
	}
	aaerror(" must have a label for looping ");
	sprintf(line," where is >%s< label ?\n",label);
	aaerror( line );
	return 0;
}
