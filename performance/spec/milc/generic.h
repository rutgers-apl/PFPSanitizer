#ifndef _GENERIC_H
#define _GENERIC_H
/************************ generic.h *************************************
*									*
*  Macros and declarations for miscellaneous generic routines           *
*  This header is for codes that call generic routines                  *
*  MIMD version 6 							*
*									*
*/

/* Other generic directory declarations are elsewhere:

   For com_*.c, see comdefs.h
   For io_lat4.c io_ansi.c, io_nonansi.c, io_piofs.c, io_romio.c see io_lat.h
   For io_wb3.c, see io_wb.h
*/

#include "int32type.h"
#include "complex.h"
#include "macros.h"
#include "random.h"

/* ax_gauge.c */
void ax_gauge();

/* bsd_sum.c */
int32type bsd_sum (char *data,int32type total_bytes);

/* check_unitarity.c */
double check_unitarity( void );

/* d_plaq?.c */
void d_plaquette(double *ss_plaq,double *st_plaq);

/* gaugefix.c and gaugefix2.c */
void gaugefix(int gauge_dir,double relax_boost,int max_gauge_iter,
	      double gauge_fix_tol, field_offset diffmat, field_offset sumvec,
	      int nvector, field_offset vector_offset[], int vector_parity[],
	      int nantiherm, field_offset antiherm_offset[], 
	      int antiherm_parity[] );

/* gauge_stuff.c */
double imp_gauge_action();
void imp_gauge_force( double eps, field_offset mom_off );
void make_loop_table();
void dsdu_qhb_subl(int dir, int subl);

/* hvy_pot.c */
void hvy_pot( field_offset links );

/* layout_*.c */
void setup_layout( void );
int node_number(int x,int y,int z,int t);
int node_index(int x,int y,int z,int t);
int num_sites(int node);

/* make_lattice.c */
void make_lattice();

/* make_global_fields.c */
void make_global_fields();

/* path_product.c */
void path_product( int *dir, int length);
void path_prod_subl(int *dir, int length, int subl);

/* plaquette4.c */
void plaquette(double *ss_plaq,double *st_plaq);

/* ploop?.c */
complex ploop( void );

/* ploop_staple.c */
complex ploop_staple(double alpha_fuzz);

/* rand_gauge.c */
void rand_gauge(field_offset G);

/* ranmom.c */
void ranmom( void );

/* ranstuff.c */
void initialize_prn(double_prn *prn_pt, int seed, int index);
double myrand(double_prn *prn_pt);

/* restrict_fourier.c */
void setup_restrict_fourier( int *key, int *slice);
void restrict_fourier( 
     field_offset src,	 /* src is field to be transformed */
     field_offset space, /* space is working space, same size as src */
     field_offset space2,/* space2 is working space, same size as src */
                         /* space2 is needed only for non power of 2 */
     int size,		 /* Size of field in bytes.  The field must
			    consist of size/sizeof(complex) consecutive
			    complex numbers.  For example, an su3_vector
			    is 3 complex numbers. */
     int isign);	 /* 1 for x -> k, -1 for k -> x */

/* reunitarize2.c */
void reunitarize( void );
int reunit_su3(su3_matrix *c);

/* smearing.c */
void smearing( void );

#endif	/* _GENERIC_H */
