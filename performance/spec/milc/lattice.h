#ifndef _LATTICE_H
#define _LATTICE_H
/****************************** lattice.h ********************************/

/* include file for MIMD version 6
   This file defines global scalars and the fields in the lattice.

   Directory for dynamical improved KS action.  Allow:
	arbitrary paths in quark action
	arbitrary paths in gauge action (eg Symanzik imp.)

   If "FN" is defined,
     Includes storage for Naik improvement (longlink[4], templongvec[4],
     gen_pt[16], etc.
     !!! Excludes storage for "fatlinks"  and "longlinks".  These are
	now allocated as field major objects at the end of make_lattice
*/

#include "defines.h"
#include "generic_quark_types.h"
#include "random.h"
#include "macros.h"  /* For MAXFILENAME */
#include "io_lat.h"    /* For gauge_file */

/* Begin definition of site structure */

#include "su3.h"
#include "random.h"   /* For double_prn */

/* The lattice is an array of sites.  */
typedef struct {
    /* The first part is standard to all programs */
	/* coordinates of this site */
	short x,y,z,t;
	/* is it even or odd? */
	char parity;
	/* my index in the array */
	int index;
#ifdef SITERAND
	/* The state information for a random number generator */
	double_prn site_prn;
	/* align to double word boundary (kludge for Intel compiler) */
	int space1;
#endif

/* ------------------------------------------------------------ */
/*   Now come the physical fields, program dependent            */
/* ------------------------------------------------------------ */
	/* gauge field */
	su3_matrix link[4];	/* the fundamental field */
#if ! defined(DSLASH_TMP_LINKS) && defined(FN)
	su3_matrix longlink[4];	/* three link straight paths */
	su3_matrix fatlink[4];	/* link plus 3-link staples */
#endif

#ifdef HMC_ALGORITHM
 	su3_matrix old_link[4];
	/* For accept/reject */
#endif

	/* antihermitian momentum matrices in each direction */
 	anti_hermitmat mom[4];

	/* The Kogut-Susskind phases, which have been absorbed into 
		the matrices.  Also the antiperiodic boundary conditions.  */
 	double phase[4];

	/* 3 element complex vectors */
 	su3_vector phi;		/* Gaussian random source vector */
 	su3_vector resid;	/* conjugate gradient residual vector */
 	su3_vector cg_p;	/* conjugate gradient change vector */
 	su3_vector xxx;		/* solution vector = Kinverse * phi */
 	su3_vector ttt;		/* temporary vector, for K*ppp */
 	su3_vector g_rand;	/* Gaussian random vector*/
	/* Use trick of combining xxx=D^adj D)^(-1) on even sites with
	   Dslash time this on odd sites when computing fermion force */
	
#ifdef PHI_ALGORITHM
 	su3_vector old_xxx;	/* For predicting next xxx */
#endif
#ifdef SPECTRUM
	su3_vector propmat[3];	/* For three source colors */
	su3_vector propmat2[3];	/* nl_spectrum() */
	su3_matrix tempmat2;
	/* for spectrum_imp() */
	/**su3_vector quark_source, quark_prop, anti_prop;**/
#define quark_source propmat2[0]
#define quark_prop propmat2[1]
#define anti_prop propmat2[2]
#endif

	/* temporary vectors and matrices */
	su3_vector tempvec[4];	/* One for each direction */
#ifdef FN
	su3_vector templongvec[4];	/* One for each direction */
        su3_vector templongv1;
#endif
	su3_matrix tempmat1,staple;
} site;

/* End definition of site structure */

/* Definition of globals */

#ifdef CONTROL
#define EXTERN 
#else
#define EXTERN extern
#endif

/* The following are global scalars 
   beta is overall gauge coupling factor (6/g^2)
   mass is quark mass
   u0 is tadpole improvement factor, perhaps (plaq/3)^(1/4)
*/
EXTERN	int nx,ny,nz,nt;	/* lattice dimensions */
EXTERN  int volume;		/* volume of lattice = nx*ny*nz*nt */
EXTERN	int iseed;		/* random number seed */
EXTERN	int warms,trajecs,steps,niter,propinterval,nflavors;
EXTERN	double epsilon;
EXTERN  double beta,mass,u0;
EXTERN	double rsqmin,rsqprop;
EXTERN	int startflag;	/* beginning lattice: CONTINUE, RELOAD, RELOAD_BINARY,
			   RELOAD_CHECKPOINT, FRESH */
EXTERN	int saveflag;	/* do with lattice: FORGET, SAVE, SAVE_BINARY,
			   SAVE_CHECKPOINT */
EXTERN	char startfile[MAXFILENAME],savefile[MAXFILENAME];
EXTERN	int total_iters;
EXTERN  int phases_in; /* 1 if KS and BC phases absorbed into matrices */
EXTERN  int source_start, source_inc, n_sources;
        /* source time, increment for it, and number of source slices */

/* Some of these global variables are node dependent */
/* They are set in "make_lattice()" */
EXTERN	int sites_on_node;		/* number of sites on this node */
EXTERN	int even_sites_on_node;	/* number of even sites on this node */
EXTERN	int odd_sites_on_node;	/* number of odd sites on this node */
EXTERN	int number_of_nodes;	/* number of nodes in use */
EXTERN  int this_node;		/* node number of this node */

/* Flags to help us remember what we are doing */
EXTERN int valid_longlinks;
EXTERN int valid_fatlinks;

EXTERN gauge_file *startlat_p;

/* Each node maintains a structure with the pseudorandom number
   generator state */
EXTERN double_prn node_prn ;


/* The lattice is a single global variable - (actually this is the
   part of the lattice on this node) */
EXTERN site *lattice;

/* Vectors for addressing */
/* Generic pointers, for gather routines */
#define N_POINTERS 16
EXTERN char ** gen_pt[N_POINTERS];

#ifdef DSLASH_TMP_LINKS
/* field major storage DON't FORGET to MALLOC somewhere */
EXTERN su3_matrix *t_longlink;
EXTERN su3_matrix *t_fatlink;
#endif

#endif /* _LATTICE_H */
