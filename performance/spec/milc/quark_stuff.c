/****** quark_stuff.c  -- ******************/
/* MIMD version 6 */
/* quark action stuff for improved action
 * D.T. 1/28/98, starting from gauge_stuff.c
 * K.O. 3/99 Added optimized fattening for Asq actions
 * D.T. 4/99 Combine force calculations for both mass quarks
 * K.O. 4/99 Optimized force for Asq action
 * S.G. 7/01, modified to use t_longlink and t_fatlink
 * C.D. 10/02, consolidated quark_stuff.c and quark_stuff_tmp.c
 *
 * This code combines quark_stuff.c and quark_stuff_tmp.c
 * with DSLASH_TMP_LINKS defined, puts links in field-major variables
 * t_longlink and t_fatlink. Otherwise, puts them in the site structure.
 *
 * In this directory, assume all paths connect even to odd sites, etc.
 * Tabulate "backwards" paths (e.g. "XDOWN" is backward path to "XUP")
 * as separate parity transforms of the fundamental paths.  They will
 * generally need a negative sign in Dslash.  See bottom for a long
 * comment on sign conventions.
 */

/**#define FFTIME**/
/**#define LLTIME**/

#include "generic_ks_includes.h"	/* definitions files and prototypes */


#define NULL_FP -1 /* NULL field_offset to be used in the optimized version *
                    * of the load_fatlinks subroutine */

/* Specify paths in orientation in which they appear in the
   forward part of the x component of dslash().  Rotations and
   reflections will be automatically included. Be careful
   about signs of coefficients.  See long comment at bottom. */
#include <quark_action.h>
/* Include file specifies the basic paths */

#define GOES_FORWARDS(dir) (dir<=TUP)
#define GOES_BACKWARDS(dir) (dir>TUP)
static void test_slice(){
  __asm__ __volatile__ ("");
}
static void start_slice(){
  __asm__ __volatile__ ("");
}
static void end_slice(){
  __asm__ __volatile__ ("");
}

void printpath( int *path, int length );
void path_transport( field_offset src, field_offset dest, int parity,
    int *dir, int length );
void path_transport_hwv( field_offset src, field_offset dest, int parity,
    int *dir, int length );
#ifdef  ASQ_OPTIMIZED_FATTENING
void compute_gen_staple(field_offset staple, int mu, int nu,
    field_offset link, double coef ) ;
#endif
#ifdef  ASQ_OPTIMIZED_FORCE
#ifndef FN
BOMB THE COMPILE
#endif
void u_shift_fermion(su3_vector *src, su3_vector *dest, int dir ) ;
void add_force_to_mom(su3_vector *back, su3_vector *forw, int dir, double coef);
void side_link_force(int mu, int nu, double coeff, su3_vector *Path,
    su3_vector *Path_nu, su3_vector *Path_mu, 
    su3_vector *Path_numu) ;

void u_shift_hw_fermion(half_wilson_vector *src, 
    half_wilson_vector *dest, int dir ) ;
void add_3f_force_to_mom(half_wilson_vector *back,
    half_wilson_vector *forw, int dir, double coeff[2]) ;
void side_link_3f_force(int mu, int nu, double coeff[2], 
    half_wilson_vector *Path   , 
    half_wilson_vector *Path_nu, 
    half_wilson_vector *Path_mu, 
    half_wilson_vector *Path_numu) ;
#endif

int path_num[MAX_BASIC_PATHS];	/* number of rotations/reflections for each 
                                   kind */
static double act_path_coeff[MAX_BASIC_PATHS]; /* actual path coefficient     *
                                                * it is equal to path_coeff   *
                                                * if not tadpole improvement  *
                                                * is specified                *
                                                * or path_coeff*u_0^(L-1) when*
                                                * tadpole improvement is      *
                                                * specified                   */

/* Array of structures, for each rotation and reflection of each kind of
   path.  */
struct {
  int dir[MAX_LENGTH];	/* directions in path */
  int length;		/* length of path */
  double coeff;	/* coefficient, including minus sign if backwards */
  double forwback;	/* +1 if in forward Dslash, -1 if in backward */
}   q_paths[MAX_NUM];
int num_q_paths;	/* number of paths in dslash */
int num_basic_paths;	/* number of paths before rotation/reflection */

int is_path_equal( int *path1, int* path2, int length );
int add_basic_path( int *vec, int length, double coeff );

/* Make table of paths in action */
void make_path_table() {

  int i,j;
#ifdef TADPOLE_IMPROVE
  int k;
#endif

  /* table of directions, 1 for each kind of path */
  /**int path_ind[MAX_BASIC_PATHS][MAX_LENGTH];**/
  /* table of coefficients in action, for each path */

  node0_printf("%s\n",quark_action_description);
  num_q_paths = 0;
  num_basic_paths = 0;


  /* add rots. and reflects to table, print out the path coefficients */
  node0_printf("path coefficients: npath  path_coeff  multiplicity\n");
  for(j=0;j<quark_action_npaths;j++) {
    double this_coeff;
    this_coeff = path_coeff[j];
#ifdef TADPOLE_IMPROVE
    for(k=1;k< path_length_in[j];k++)this_coeff /= u0;
#endif
    act_path_coeff[j] = this_coeff ;
    i = add_basic_path( path_ind[j], path_length_in[j],
        this_coeff );
    node0_printf("                    %d      %e     %d\n",
        j,this_coeff,i);
  }
}

/* add rotations and reflections of a path to the table.  Return
   multiplicity of paths added */
int add_basic_path( int *basic_vec, int length, double coeff ) {

  int perm[8],pp[8],ir[4];
  int j,path_num;
  int vec[MAX_LENGTH];
  int flag;

  path_num = 0;
  /* now fill the long table with all rotations and reflections
     of the fundamental path.  The path presented to us is for
     the positive x component of dslash, so if the x coordinate
     is reflected it will appear with a negative sign. */
  /* permutations */
  for(perm[0]=0;perm[0]<4;perm[0]++)
    for(perm[1]=0;perm[1]<4;perm[1]++)
      for(perm[2]=0;perm[2]<4;perm[2]++)
        for(perm[3]=0;perm[3]<4;perm[3]++){
          if(perm[0] != perm[1] && perm[0] != perm[2] 
              && perm[0] != perm[3] && perm[1] != perm[2]
              && perm[1] != perm[3] && perm[2] != perm[3] ) {
            /* reflections*/
            for(ir[0]=0;ir[0]<2;ir[0]++)
              for(ir[1]=0;ir[1]<2;ir[1]++)
                for(ir[2]=0;ir[2]<2;ir[2]++)
                  for(ir[3]=0;ir[3]<2;ir[3]++){
                    for(j=0;j<4;j++){
                      pp[j]=perm[j];

                      if(ir[j] == 1) pp[j]=OPP_DIR(pp[j]);
                      pp[OPP_DIR(j)]=OPP_DIR(pp[j]);
                    }
                    /* create new vector*/
                    for(j=0;j<length;j++) vec[j]=pp[basic_vec[j]];
                    for(j=length;j<MAX_LENGTH;j++) vec[j]=NODIR;

                    flag=0;
                    /* check if it's a new set: */
                    for(j=0;j<num_q_paths;j++){
                      flag = is_path_equal( vec, q_paths[j].dir, MAX_LENGTH );
                      if(flag==1)break;
                    }
                    if(flag == 0 ){
                      if(num_q_paths>=MAX_NUM){
                        node0_printf("OOPS: MAX_NUM too small\n");
                        exit(0);
                      }
                      q_paths[num_q_paths].length=length;
                      for(j=0;j<MAX_LENGTH;j++) q_paths[num_q_paths].dir[j]=vec[j];
                      /* remember to copy NODIR's, or comparison will get confused */
                      if(ir[0]==0){
                        q_paths[num_q_paths].coeff =  coeff + 0.0f;
                        q_paths[num_q_paths].forwback =  +1;
                      }
                      else{
                        q_paths[num_q_paths].coeff = -coeff;
                        q_paths[num_q_paths].forwback = -1;
                      }
                      num_q_paths++;
                      path_num++;
                      /**node0_printf("ADD PATH %d:  rx=%d ",num_q_paths-1,ir[0]);
                        printpath( vec, length );**/
                    }

                  } /* end reflection*/
          } /* end permutation if block */
        } /* end permutation */
  num_basic_paths++;
  return(path_num);
} /* add_basic_path */

/* parallel transport a vector to the current site along a path.
   For example, if the path is "XUP", bring in the vector from
   the +X direction to the current site. If KS phases are in lattice,
   this transport will automatically include them. 
   OK for src and dest to be the same.  OK for length=0.  */
/* NOT OPTIMIZED at the moment - do lots of extra copying.  Use temp.
   vectors rather than stuff in lattice.h */
void path_transport( field_offset src, field_offset dest, int parity,
    int *dir, int length ){
  register int i;
  register site *s;
  msg_tag *mtag0;
  int j;
  su3_vector *tmp_src,*tmp_dest,*tmp_work; /*source, dest and workspace*/
  su3_vector *tmp_pt; /* scratch */
  int tmp_parity, tmp_otherparity; /* parity for this step */

  if( length > 0 ){
    tmp_src = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    tmp_dest = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    tmp_work = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));

    for( j=length-1; j>=0; j-- ){
      /* figure out parities for this step */
      if( j%2==0 ){
        tmp_parity = parity;
        switch(tmp_parity){
          case EVEN: tmp_otherparity=ODD; break;
          case ODD: tmp_otherparity=EVEN; break;
          case EVENANDODD: tmp_otherparity=EVENANDODD; break;
        }
      }
      else { /* odd # step */
        tmp_otherparity = parity;
        switch(tmp_otherparity){
          case EVEN: tmp_parity=ODD; break;
          case ODD: tmp_parity=EVEN; break;
          case EVENANDODD: tmp_parity=EVENANDODD; break;
        }
      }

      if( j==length-1 ){
        FORSOMEPARITY(i,s,tmp_otherparity){
          tmp_src[i] = *(su3_vector *)F_PT(s,src);
        }
      }

      if( GOES_FORWARDS(dir[j]) ) {
        mtag0 = start_gather_from_temp( tmp_src, sizeof(su3_vector),
            dir[j], tmp_parity, gen_pt[0] );
        wait_gather(mtag0);
        FORSOMEPARITY(i,s,tmp_parity){
          mult_su3_mat_vec( &(s->link[dir[j]]),
              (su3_vector *)(gen_pt[0][i]),
              &(tmp_dest[i]) );
        }
        cleanup_gather(mtag0);
      }

      else{ /* GOES_BACKWARDS(dir[j]) */
        FORSOMEPARITY(i,s,tmp_otherparity){
          mult_adj_su3_mat_vec( &(s->link[OPP_DIR(dir[j])]),
              &(tmp_src[i]), &(tmp_work[i]) );
        }
        mtag0 = start_gather_from_temp( tmp_work, sizeof(su3_vector),
            dir[j], tmp_parity, gen_pt[0] );
        wait_gather(mtag0);
        FORSOMEPARITY(i,s,tmp_parity){
          tmp_dest[i] = *(su3_vector *)gen_pt[0][i];
        }
        cleanup_gather(mtag0);
      }

      /* src for next step is dest for this one. */
      tmp_pt=tmp_src; tmp_src=tmp_dest; tmp_dest=tmp_pt;
    }  /* j=link in path */
    /* done, copy result into real dest. (tmp_src now points to result) */
    FORSOMEPARITY(i,s,parity){
      *(su3_vector *)F_PT(s,dest) = tmp_src[i];
    }
    free(tmp_src); free(tmp_dest); free(tmp_work);
  } /* end if(length>0) */
  else if( src != dest ){ /* for length=0 */
    FORSOMEPARITY(i,s,parity){
      *(su3_vector *)F_PT(s,dest) = *(su3_vector *)F_PT(s,src);
    }
  }
} /* path_transport */

/* Path transport a half_wilson_vector */
void path_transport_hwv( field_offset src, field_offset dest, int parity,
    int *dir, int length ){
  register int i;
  register site *s;
  msg_tag *mtag0;
  int j;
  half_wilson_vector *tmp_src,*tmp_dest,*tmp_work; /*source, dest and workspace*/
  half_wilson_vector *tmp_pt; /* scratch */
  int tmp_parity, tmp_otherparity; /* parity for this step */

  if( length > 0 ){
    tmp_src = (half_wilson_vector *)calloc(
        sites_on_node, sizeof(half_wilson_vector));
    tmp_dest = (half_wilson_vector *)calloc(
        sites_on_node, sizeof(half_wilson_vector));
    tmp_work = (half_wilson_vector *)calloc(
        sites_on_node, sizeof(half_wilson_vector));

    for( j=length-1; j>=0; j-- ){
      /* figure out parities for this step */
      if( j%2==0 ){
        tmp_parity = parity;
        switch(tmp_parity){
          case EVEN: tmp_otherparity=ODD; break;
          case ODD: tmp_otherparity=EVEN; break;
          case EVENANDODD: tmp_otherparity=EVENANDODD; break;
        }
      }
      else { /* odd # step */
        tmp_otherparity = parity;
        switch(tmp_otherparity){
          case EVEN: tmp_parity=ODD; break;
          case ODD: tmp_parity=EVEN; break;
          case EVENANDODD: tmp_parity=EVENANDODD; break;
        }
      }

      if( j==length-1 ){
        FORSOMEPARITY(i,s,tmp_otherparity){
          tmp_src[i] = *(half_wilson_vector *)F_PT(s,src);
        }
      }

      if( GOES_FORWARDS(dir[j]) ) {
        mtag0 = start_gather_from_temp( tmp_src,
            sizeof(half_wilson_vector), dir[j], tmp_parity, gen_pt[0] );
        wait_gather(mtag0);
        FORSOMEPARITY(i,s,tmp_parity){
          mult_su3_mat_hwvec( &(s->link[dir[j]]),
              (half_wilson_vector *)(gen_pt[0][i]),
              &(tmp_dest[i]) );
        }
        cleanup_gather(mtag0);
      }

      else{ /* GOES_BACKWARDS(dir[j]) */
        FORSOMEPARITY(i,s,tmp_otherparity){
          mult_adj_su3_mat_hwvec( &(s->link[OPP_DIR(dir[j])]),
              &(tmp_src[i]), &(tmp_work[i]) );
        }
        mtag0 = start_gather_from_temp( tmp_work,
            sizeof(half_wilson_vector), dir[j], tmp_parity, gen_pt[0] );
        wait_gather(mtag0);
        FORSOMEPARITY(i,s,tmp_parity){
          tmp_dest[i] = *(half_wilson_vector *)gen_pt[0][i];
        }
        cleanup_gather(mtag0);
      }

      /* src for next step is dest for this one. */
      tmp_pt=tmp_src; tmp_src=tmp_dest; tmp_dest=tmp_pt;
    }  /* j=link in path */
    /* done, copy result into real dest. (tmp_src now points to result) */
    FORSOMEPARITY(i,s,parity){
      *(half_wilson_vector *)F_PT(s,dest) = tmp_src[i];
    }
    free(tmp_src); free(tmp_dest); free(tmp_work);
  } /* end if(length>0) */
  else if( src != dest ){ /* for length=0 */
    FORSOMEPARITY(i,s,parity){
      *(half_wilson_vector *)F_PT(s,dest) =
        *(half_wilson_vector *)F_PT(s,src);
    }
  }
} /* path_transport_hwv */


#ifdef EO
/* Stupid dslash routine that follows all the paths.
   Should optimize by precomputing sums of paths to all the displacements
   Use dslash_fn for actions that involve only +X and +X+X+X couplings.
 */
/* D_slash routine - sets dest. on each site equal to sum of
   sources parallel transported to site, with minus sign for transport
   from negative directions.  */
void dslash_eo( field_offset src, field_offset dest, int parity ) {
  register int i;
  register site *s;
  register int ipath,otherparity;
  register double x; /* coefficient of path */

  switch(parity){
    case EVEN:	otherparity=ODD; break;
    case ODD:	otherparity=EVEN; break;
    case EVENANDODD:	otherparity=EVENANDODD; break;
  }

  /* Parallel transport by all the paths in the action.  
     Multiply by coefficient in table
   */
  FORSOMEPARITY(i,s,parity){ clearvec( (su3_vector *)F_PT(s,dest) ); }

  for( ipath=0; ipath<num_q_paths; ipath++ ){  /* loop over paths */
    path_transport( src, F_OFFSET(tempvec[0]), parity,
        q_paths[ipath].dir, q_paths[ipath].length );
    x=q_paths[ipath].coeff;
    FORSOMEPARITY(i,s,parity){
      scalar_mult_add_su3_vector(  (su3_vector *)F_PT(s,dest),
          &(s->tempvec[0]), x, (su3_vector *)F_PT(s,dest) );
    }
  }   /* ipath */
} /* dslash_eo */
#endif /* ifdef EO */

#ifdef FN
/* Sum over paths connecting to nearest neighbor point (fat link) and to third
   nearest neighbor (longlinks) */
/* Doug Toussaint 2/4/98 */
/* modified to use t_longlinks, S. Gottlieb 7/13/01 */
/* long link calculating routine */
/* path_product() follows the path starting at step 0, and
   leaves the answer at the end of the path.  We want the answer
   at the site where the path begins.  So we look for paths with
   the opposite displacement from the displacement of the point
   that we want to transport to this site, and take the adjoint
   of the matrix at the end. clear? */
/* KS phases and APBC must be in the links. See long comment at bottom*/
void load_longlinks() {
  register int i;
  register site *s;
  int ipath,dir;
  int disp[4];
  register su3_matrix *long1;

#ifdef LLTIME
  double dtime;
  dtime=-dclock();
#endif
  if( phases_in != 1){
    node0_printf("BOTCH: load_longlinks needs phases in\n");
    terminate(0);
  }
  for (dir=XUP; dir<=TUP; dir++){ /* loop over longlink directions */
    /* set longlink to zero */
    FORALLSITES(i,s){
#ifdef DSLASH_TMP_LINKS
      long1 = &(t_longlink[4*i+dir]);
#else
      long1 = &(s->longlink[dir]);
#endif
      clear_su3mat( long1 );
    }

    /* loop over paths, checking for ones with total displacement 3*dir */
    for( ipath=0; ipath<num_q_paths; ipath++ ){  /* loop over paths */
      /* compute total displacement of path */
      for(i=XUP;i<=TUP;i++)disp[i]=0;
      for( i=0; i<q_paths[ipath].length; i++){
        if( GOES_FORWARDS(q_paths[ipath].dir[i]) )
          disp[        q_paths[ipath].dir[i]  ]++;
        else
          disp[OPP_DIR(q_paths[ipath].dir[i]) ]--;
      }
      for( disp[dir]+=3,i=XUP; i<=TUP; i++)if(disp[i]!=0)break;
      if( i<=TUP )continue;  /* skip if path doesn't go to right place */
      /**printf("ipath = %d, found a path:  ",ipath);
        for(j=0;j<q_paths[ipath].length;j++)printf("\t%d", q_paths[ipath].dir[j]);
        printf("\n");**/

      path_product( q_paths[ipath].dir, q_paths[ipath].length );
      FORALLSITES(i,s){
        su3_adjoint( &(s->tempmat1), &(s->staple) );
#ifdef DSLASH_TMP_LINKS
        long1 = &(t_longlink[4*i+dir]);
#else
        long1 = &(s->longlink[dir]);
#endif
        scalar_mult_add_su3_matrix( long1,
            &(s->staple), -q_paths[ipath].coeff, long1 );
        /* minus sign in coeff. because we used backward path*/
      }
    } /* ipath */

  } /* loop over directions */

  valid_longlinks = 1;
#ifdef LLTIME
  dtime += dclock();
  node0_printf("LLTIME(Long):  %e\n",dtime);
#endif
}  /* load_longlinks() */

/* KS phases and APBC must be in the links. See long comment at bottom*/
void load_fatlinks() {
  register int i;
  register site *s;
  int dir;
  register su3_matrix *fat1;
#ifdef ASQ_OPTIMIZED_FATTENING
  int  nu,rho,sig ;
  double one_link ; /*needed to fix the problem with the Lepage term */
#else
  int ipath;
  int disp[4];
#endif

#ifdef LLTIME
  double dtime;
  dtime=-dclock();
#endif
  if( phases_in != 1){
    node0_printf("BOTCH: load_fatlinks needs phases in\n");
    terminate(0);
  }

#ifndef  ASQ_OPTIMIZED_FATTENING   /* general case code */
  for (dir=XUP; dir<=TUP; dir++){ /* loop over fatlink directions */
    /* set fatlink to zero */
    FORALLSITES(i,s){
#ifdef DSLASH_TMP_LINKS
      fat1 = &(t_fatlink[4*i+dir]);
#else
      fat1 = &(s->fatlink[dir]);
#endif
      clear_su3mat( fat1 );
    }

    /* loop over paths, checking for ones with total displacement 1*dir */
    for( ipath=0; ipath<num_q_paths; ipath++ ){  /* loop over paths */
      /* compute total displacement of path */
      for(i=XUP;i<=TUP;i++)disp[i]=0;
      for( i=0; i<q_paths[ipath].length; i++){
        if( GOES_FORWARDS(q_paths[ipath].dir[i]) )
          disp[        q_paths[ipath].dir[i]  ]++;
        else
          disp[OPP_DIR(q_paths[ipath].dir[i]) ]--;
      }
      for( disp[dir]+=1,i=XUP; i<=TUP; i++)if(disp[i]!=0)break;
      if( i<=TUP )continue;  /* skip if path doesn't go to right place */
      /**printf("dir = %d, found a path:  ",dir);
        for(j=0;j<q_paths.[ipath].length;j++)printf("\t%d", q_paths[ipath].dir[j]);
        printf("\n");**/

      path_product( q_paths[ipath].dir, q_paths[ipath].length );
      FORALLSITES(i,s){
        su3_adjoint( &(s->tempmat1), &(s->staple) );
#ifdef DSLASH_TMP_LINKS
        fat1 = &(t_fatlink[4*i+dir]);
#else
        fat1 = &(s->fatlink[dir]);
#endif
        scalar_mult_add_su3_matrix( fat1,
            &(s->staple), -q_paths[ipath].coeff, fat1 );
        /* minus sign in coeff. because we used backward path*/
      }
    } /* ipath */
  } /* loop over directions */
#else	/* ASQ_OPTIMIZED_FATTENING, for Asq and Asqtad actions */
  /*  Optimized fattening code for the Asq and Asqtad actions.           *
   * I assume that path 0 is the one link path 2 the 3-staple            *
   * path 3 the 5-staple path 4 the 7-staple and path 5 the Lapage term. *
   * Path 1 is the Naik term.                                            */

  /* to fix up the Lepage term, included by a trick below */
  one_link = (act_path_coeff[0] - 6.0*act_path_coeff[5]) ; 

  for (dir=XUP; dir<=TUP; dir++){
    FORALLSITES(i,s) /* Intialize fat links with c_1*U_\mu(x) */
    {
#ifdef DSLASH_TMP_LINKS
      fat1 = &(t_fatlink[4*i+dir]);
#else
      fat1 = &(s->fatlink[dir]);
#endif
      scalar_mult_su3_matrix(&(s->link[dir]), one_link ,
          fat1 );
    }
    for(nu=XUP; nu<=TUP; nu++) if(nu!=dir)
    {
      compute_gen_staple(F_OFFSET(staple),dir,nu,F_OFFSET(link[dir]),
          act_path_coeff[2]);
      /* The Lepage term */
      /* Note this also involves modifying c_1 (above) */
      compute_gen_staple(NULL_FP,dir,nu,F_OFFSET(staple),act_path_coeff[5]);
      for(rho=XUP; rho<=TUP; rho++) if((rho!=dir)&&(rho!=nu))
      {
        compute_gen_staple(F_OFFSET(tempmat1),dir,rho,F_OFFSET(staple),
            act_path_coeff[3]);
        for(sig=XUP; sig<=TUP; sig++)
          if((sig!=dir)&&(sig!=nu)&&(sig!=rho))
          {
            compute_gen_staple(NULL_FP,dir,sig,
                F_OFFSET(tempmat1),
                act_path_coeff[4]);
          } /* sig */
      } /* rho */
    } /* nu */
  }/* dir */  
#endif

  valid_fatlinks = 1;
#ifdef LLTIME
  dtime += dclock();
  node0_printf("LLTIME(Fat):  %e\n",dtime);
#endif
}  /* load_fatlinks() */
#endif /* ifdef FN */


/* compare two paths, return 1 if equal, else zero */
int is_path_equal( int *path1, int* path2, int length ){
  register int i;
  for(i=0;i<length;i++)if(path1[i]!=path2[i])return(0);
  return(1);
}


/* update the  momenta with the fermion force */
/* Assumes that the conjugate gradient has been run, with the answer in
   x_off, and dslash(x_off,x_off,ODD) has been run. (fills in x_off_odd) */
/* SEE LONG COMMENTS AT END */

#ifndef ASQ_OPTIMIZED_FORCE
void eo_fermion_force( double eps, int nflavors, field_offset x_off ){
  /* note CG_solution and Dslash * solution are combined in "x_off" */
  /* New version 1/21/99.  Use forward part of Dslash to get force */
  /* see long comment at end */
  /* For each link we need x_off transported from both ends of path. */
  register int i,dir,lastdir,ipath,ilink;
  register site *s;
  int length;
  su3_matrix tmat,tmat2;
  double ferm_epsilon, coeff;
#ifdef FFTIME
  double dtime;
#endif
  msg_tag *mtag0;
  half_wilson_vector *hw_tmp0,*hw_tmp1,*tmp_pt;

#ifdef FFTIME
  dtime=-dclock();
#endif
  ferm_epsilon = 2.0*(nflavors/4.0)*eps;
  hw_tmp0 = (half_wilson_vector *)
    calloc(sites_on_node, sizeof(half_wilson_vector));
  hw_tmp1 = (half_wilson_vector *)
    calloc(sites_on_node, sizeof(half_wilson_vector));
  /* Use half_wilson_vectors to store x_off transported from ends of
     path.  0 component from forward end, 1 component from back end */

  /* loop over paths, and loop over links in path */
  for( ipath=0; ipath<num_q_paths; ipath++ ){
    if(q_paths[ipath].forwback== -1)continue;	/* skip backwards dslash */
    length = q_paths[ipath].length;

    /* path transport x_off and Dslash*x_off from far end.  Sometimes
       we need them at the start point of the path, and sometimes
       one link into the path - an optimization for later */
    path_transport( x_off, F_OFFSET(tempvec[0]),
        EVENANDODD, q_paths[ipath].dir, length );
    /* use tempvec[1] for transport from starting end */
    FORALLSITES(i,s){
      hw_tmp0[i].h[0]=s->tempvec[0];
      hw_tmp0[i].h[1]=*(su3_vector *)F_PT(s,x_off);
    }

    /* A path has (length+1) points, counting the ends.  At first
       point, no "down" direction links have their momenta "at this
       point". At last, no "up" ... */
    for( ilink=0; ilink<=length; ilink++ ){
      if(ilink<length)dir = q_paths[ipath].dir[ilink];
      else dir=NODIR;
      coeff = ferm_epsilon*q_paths[ipath].coeff;
      if( (ilink%2)==1 )coeff = -coeff;

      /* path transport x_off and Dslash*x_off from previous point */
      /* Use "half_wilson_vector" to handle pair of vectors -
         0 component is x_off from forward end, 1 component from back end */
      /* sometimes we don't need them */
      if( (ilink>0&&ilink<length) || 
          (ilink==length && GOES_BACKWARDS(lastdir)) ){

        if( GOES_FORWARDS(lastdir) ){
          FORALLSITES(i,s){
            mult_adj_su3_mat_hwvec( &(s->link[lastdir]),
                &(hw_tmp0[i]), &(hw_tmp1[i]) );
          }
          mtag0 = start_gather_from_temp( hw_tmp1, 2*sizeof(su3_vector),
              OPP_DIR(lastdir), EVENANDODD, gen_pt[0] );
          wait_gather(mtag0);
          FORALLSITES(i,s){
            hw_tmp0[i] = *(half_wilson_vector *)gen_pt[0][i];
          }
          cleanup_gather(mtag0);
        }
        else{   /* GOES_BACKWARDS(lastdir) */
          mtag0 = start_gather_from_temp( hw_tmp0, 2*sizeof(su3_vector),
              OPP_DIR(lastdir), EVENANDODD, gen_pt[0] );
          wait_gather(mtag0);
          FORALLSITES(i,s){
            mult_su3_mat_hwvec( &(s->link[OPP_DIR(lastdir)]),
                (half_wilson_vector *)(gen_pt[0][i]),
                &(hw_tmp1[i]) );
          }
          tmp_pt = hw_tmp0; hw_tmp0 = hw_tmp1; hw_tmp1 = tmp_pt;
          cleanup_gather(mtag0);
        }
      }

      /* add in contribution to the force */
      /* Put antihermitian traceless part into momentum */
      FORALLSITES(i,s){
        if( ilink<length && GOES_FORWARDS(dir) ){
          uncompress_anti_hermitian( &(s->mom[dir]), &tmat2 );
          su3_projector( &(hw_tmp0[i].h[0]), &(hw_tmp0[i].h[1]), &tmat );
          if( s->parity==EVEN )scalar_mult_add_su3_matrix(
              &tmat2, &tmat,  coeff, &tmat2 );
          else		 scalar_mult_add_su3_matrix(
              &tmat2, &tmat, -coeff, &tmat2 );
          make_anti_hermitian( &tmat2, &(s->mom[dir]) );
        }
        if( ilink>0 && GOES_BACKWARDS(lastdir) ){
          uncompress_anti_hermitian( &(s->mom[OPP_DIR(lastdir)]), &tmat2 );
          su3_projector( &(hw_tmp0[i].h[0]), &(hw_tmp0[i].h[1]), &tmat );
          if( s->parity==EVEN )scalar_mult_add_su3_matrix(
              &tmat2, &tmat, -coeff, &tmat2 );
          else		 scalar_mult_add_su3_matrix(
              &tmat2, &tmat,  coeff, &tmat2 );
          make_anti_hermitian( &tmat2, &(s->mom[OPP_DIR(lastdir)]) );
        }
      }
      lastdir = dir;
    } /* end loop over links in path */
  } /* end loop over paths */

  free( hw_tmp0 ); free( hw_tmp1 );

#ifdef FFTIME
  dtime += dclock();
  node0_printf("FFTIME:  %e\n",dtime);
  /**printf("TLENGTH: %d\n",tlength);**/
#endif
} /* eo_fermion_force(version 6) */


void eo_fermion_force_3f( double eps, int nflav1, field_offset x1_off,
    int nflav2, field_offset x2_off ){
  /* note CG_solution and Dslash * solution are combined in "x_off" */
  /* New version 1/21/99.  Use forward part of Dslash to get force */
  /* 4/14/99 combine force from two different mass quarks, (eg 2+1flavors) */
  /* see long comment at end */
  /* For each link we need x_off transported from both ends of path. */
  register int i,dir,lastdir,ipath,ilink;
  register site *s;
  int length;
  su3_matrix tmat,tmat2;
  double ferm_epsilon1, ferm_epsilon2, coeff1, coeff2;
#ifdef FFTIME
  double dtime;
#endif
  msg_tag *mtag0;
  wilson_vector *w_tmp0,*w_tmp1,*tmp_pt;

#ifdef FFTIME
  dtime=-dclock();
#endif
  ferm_epsilon1 = 2.0*(nflav1/4.0)*eps;
  ferm_epsilon2 = 2.0*(nflav2/4.0)*eps;
  w_tmp0 = (wilson_vector *)
    calloc(sites_on_node, sizeof(wilson_vector));
  w_tmp1 = (wilson_vector *)
    calloc(sites_on_node, sizeof(wilson_vector));
  /* Use wilson_vectors to store x_off transported from ends of
     path.  0 and 1 components from forward end, 2 and 3 components
     from back end */

  /* loop over paths, and loop over links in path */
  for( ipath=0; ipath<num_q_paths; ipath++ ){
    if(q_paths[ipath].forwback== -1)continue;	/* skip backwards dslash */
    length = q_paths[ipath].length;

    /* path transport x_off and Dslash*x_off from far end.  Sometimes
       we need them at the start point of the path, and sometimes
       one link into the path - an optimization for later */
    /**
      path_transport( x1_off, F_OFFSET(tempvec[0]),
      EVENANDODD, q_paths[ipath].dir, length );
      path_transport( x2_off, F_OFFSET(tempvec[1]),
      EVENANDODD, q_paths[ipath].dir, length );
     **/
    /** WARNING!! Assumes xxx1 and xxx2 contiguous **/
    if( x2_off-x1_off != sizeof(su3_vector) ){node0_printf("BOTCH\n"); exit(0);}
    path_transport_hwv( x1_off, F_OFFSET(tempvec[0]),
        EVENANDODD, q_paths[ipath].dir, length );
    /* use tempvec[2] for transport from starting end */
    FORALLSITES(i,s){
      w_tmp0[i].d[0]=s->tempvec[0];
      w_tmp0[i].d[1]=s->tempvec[1];
      w_tmp0[i].d[2]=*(su3_vector *)F_PT(s,x1_off);
      w_tmp0[i].d[3]=*(su3_vector *)F_PT(s,x2_off);
    }

    /* A path has (length+1) points, counting the ends.  At first
       point, no "down" direction links have their momenta "at this
       point". At last, no "up" ... */
    for( ilink=0; ilink<=length; ilink++ ){
      if(ilink<length)dir = q_paths[ipath].dir[ilink];
      else dir=NODIR;
      coeff1 = ferm_epsilon1*q_paths[ipath].coeff;
      coeff2 = ferm_epsilon2*q_paths[ipath].coeff;
      if( (ilink%2)==1 ){ coeff1 = -coeff1; coeff2 = -coeff2;}

      /* path transport x_off and Dslash*x_off from previous point */
      /* Use "wilson_vector" to handle pair of vectors -
         0 component is x_off1 from forward end, 1 component is x_off2
         from forward end,  2 and 3  components are x_off1 and x_off2 
         from back end */
      /* sometimes we don't need them */
      if( (ilink>0&&ilink<length) || 
          (ilink==length && GOES_BACKWARDS(lastdir)) ){

        if( GOES_FORWARDS(lastdir) ){
          FORALLSITES(i,s){
            mult_adj_mat_wilson_vec( &(s->link[lastdir]),
                &(w_tmp0[i]), &(w_tmp1[i]) );
          }
          mtag0 = start_gather_from_temp( w_tmp1, 4*sizeof(su3_vector),
              OPP_DIR(lastdir), EVENANDODD, gen_pt[0] );
          wait_gather(mtag0);
          FORALLSITES(i,s){
            w_tmp0[i] = *(wilson_vector *)gen_pt[0][i];
          }
          cleanup_gather(mtag0);
        }
        else{   /* GOES_BACKWARDS(lastdir) */
          mtag0 = start_gather_from_temp( w_tmp0, 4*sizeof(su3_vector),
              OPP_DIR(lastdir), EVENANDODD, gen_pt[0] );
          wait_gather(mtag0);
          FORALLSITES(i,s){
            mult_mat_wilson_vec( &(s->link[OPP_DIR(lastdir)]),
                (wilson_vector *)(gen_pt[0][i]),
                &(w_tmp1[i]) );
          }
          tmp_pt = w_tmp0; w_tmp0 = w_tmp1; w_tmp1 = tmp_pt;
          cleanup_gather(mtag0);
        }
      }

      /* add in contribution to the force */
      /* Put antihermitian traceless part into momentum */
      FORALLSITES(i,s){
        if( ilink<length && GOES_FORWARDS(dir) ){
          if( s->parity==ODD ){coeff1 *= -1.0; coeff2 *= -1.0; }
          uncompress_anti_hermitian( &(s->mom[dir]), &tmat2 );
          su3_projector( &(w_tmp0[i].d[0]), &(w_tmp0[i].d[2]), &tmat );
          scalar_mult_add_su3_matrix( &tmat2, &tmat,  coeff1, &tmat2 );
          su3_projector( &(w_tmp0[i].d[1]), &(w_tmp0[i].d[3]), &tmat );
          scalar_mult_add_su3_matrix( &tmat2, &tmat,  coeff2, &tmat2 );
          make_anti_hermitian( &tmat2, &(s->mom[dir]) );
          if( s->parity==ODD ){coeff1 *= -1.0; coeff2 *= -1.0; }
        }
        if( ilink>0 && GOES_BACKWARDS(lastdir) ){
          if( s->parity==EVEN ){coeff1 *= -1.0; coeff2 *= -1.0; }
          uncompress_anti_hermitian( &(s->mom[OPP_DIR(lastdir)]), &tmat2 );
          su3_projector( &(w_tmp0[i].d[0]), &(w_tmp0[i].d[2]), &tmat );
          scalar_mult_add_su3_matrix( &tmat2, &tmat,  coeff1, &tmat2 );
          su3_projector( &(w_tmp0[i].d[1]), &(w_tmp0[i].d[3]), &tmat );
          scalar_mult_add_su3_matrix( &tmat2, &tmat,  coeff2, &tmat2 );
          make_anti_hermitian( &tmat2, &(s->mom[OPP_DIR(lastdir)]) );
          if( s->parity==EVEN ){coeff1 *= -1.0; coeff2 *= -1.0; }
        }
      }
      lastdir = dir;
    } /* end loop over links in path */
  } /* end loop over paths */

  free( w_tmp0 ); free( w_tmp1 );

#ifdef FFTIME
  dtime += dclock();
  node0_printf("FFTIME:  %e\n",dtime);
  /**printf("TLENGTH: %d\n",tlength);**/
#endif
} /* eo_fermion_force_3f(version 6) */



#else /* ASQ_OPTIMIZED_FORCE, for Asq and Asqtad actions */
/* Optimized force code for the Asq and Asqtad actions                 *
 * I assume that path 0 is the one link path 2 the 3-staple            *
 * path 3 the 5-staple path 4 the 7-staple and path 5 the Lapage term. *
 * Path 1 is the Naik term.                                            */
#define Pmu          tempvec[0] 
#define Pnumu        tempvec[1]
#define Prhonumu     tempvec[2]
#define P7           tempvec[3]
#define P7rho        tempvec[4]              
#define P7rhonu      tempvec[5]
#define P5           tempvec[6]
#define P3           tempvec[7]
#define P5nu         tempvec[3]
#define P3mu         tempvec[3]
#define Popmu        tempvec[4]
#define Pmumumu      tempvec[4]
void eo_fermion_force( double eps, int nflavors, field_offset x_off ){
  /* note CG_solution and Dslash * solution are combined in "x_off" */
  /* New version 1/21/99.  Use forward part of Dslash to get force */
  /* see long comment at end */
  /* For each link we need x_off transported from both ends of path. */
  register int i ;
  register site *s;
  int mu,nu,rho,sig ;
  int DirectLinks[8] ;
  double ferm_epsilon, coeff;
  double OneLink, Lepage, Naik, FiveSt, ThreeSt, SevenSt ;
  su3_vector *tempvec[8] ;
  su3_vector *temp_x ;

#ifdef FFTIME
  double dtime;

  dtime=-dclock();
#endif
  ferm_epsilon = 2.0*(nflavors/4.0)*eps;

  /* Path coefficients times fermion epsilon */
  OneLink = act_path_coeff[0]*ferm_epsilon ; 
  Naik    = act_path_coeff[1]*ferm_epsilon ;
  ThreeSt = act_path_coeff[2]*ferm_epsilon ;
  FiveSt  = act_path_coeff[3]*ferm_epsilon ;
  SevenSt = act_path_coeff[4]*ferm_epsilon ;
  Lepage  = act_path_coeff[5]*ferm_epsilon ;
  /* *************************************** */

  /* Initialize the DirectLink flags */
  for(mu=0;mu<8;mu++)
    DirectLinks[mu] = 0 ;

  /* Allocate temporary vectors */
  for(mu=0;mu<8;mu++)
    tempvec[mu] = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));

  /*copy x_off to a temporary vector */
  temp_x = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
  FORALLSITES(i,s) temp_x[i] = *(su3_vector *)F_PT(s,x_off) ;

  for(sig=0;sig<8;sig++)
  {
    for(mu=0;mu<8;mu++)if((mu!=sig)&&(mu!=OPP_DIR(sig)))
    {
      u_shift_fermion(temp_x, Pmu, OPP_DIR(mu));
      u_shift_fermion(Pmu, P3, sig);
      if(GOES_FORWARDS(sig))
      {
        /* Add the force F_sig[x+mu]:         x--+             *
         *                                   |   |             *
         *                                   o   o             *
         * the 1 link in the path: - (numbering starts form 0) */
        add_force_to_mom(P3, Pmu, sig, -ThreeSt) ;
      }
      for(nu=0;nu<8;nu++)if((nu!=mu )&&(nu!=OPP_DIR(mu ))&&
          (nu!=sig)&&(nu!=OPP_DIR(sig)))
      {
        u_shift_fermion(Pmu, Pnumu, OPP_DIR(nu));
        u_shift_fermion(Pnumu, P5, sig);
        if(GOES_FORWARDS(sig))
        {
          /* Add the force F_sig[x+mu+nu]:      x--+             *
           *                                   |   |             *
           *                                   o   o             *
           * the 2 link in the path: + (numbering starts form 0) */
          add_force_to_mom(P5, Pnumu, sig, FiveSt);
        }
        for(rho=0;rho<8;rho++)if((rho!=mu )&&(rho!=OPP_DIR(mu ))&&
            (rho!=nu )&&(rho!=OPP_DIR(nu ))&&
            (rho!=sig)&&(rho!=OPP_DIR(sig)))
        {
          u_shift_fermion(Pnumu, Prhonumu, OPP_DIR(rho));
          /* Length 7 paths */
          u_shift_fermion(Prhonumu, P7,sig);
          if(GOES_FORWARDS(sig))
          {
            /* Add the force F_sig[x+mu+nu+rho]:  x--+             *
             *                                   |   |             *
             *                                   o   o             *
             * the 3 link in the path: - (numbering starts form 0) */
            add_force_to_mom(P7, Prhonumu, sig, -SevenSt ) ;
          }
          /*Add the force F_rho the 2(4) link in the path: +     */
          u_shift_fermion(P7, P7rho, rho);
          side_link_force(rho,sig,SevenSt, Pnumu, P7, Prhonumu, P7rho);
          /* Add the P7rho vector to P5 */
          coeff = SevenSt/FiveSt ; 
          FORALLSITES(i,s)
            scalar_mult_add_su3_vector(&(P5[i]),&(P7rho[i]),coeff,
                &(P5[i]));
        }/* rho */
        /* Length 5 paths */
        /*Add the force F_nu the 1(3) link in the path: -     */
        u_shift_fermion(P5,P5nu, nu);
        side_link_force(nu,sig,-FiveSt,Pmu,P5, 
            Pnumu,P5nu) ;
        /* Add the P5nu vector to P3 */
        coeff = FiveSt/ThreeSt ; 
        FORALLSITES(i,s)
          scalar_mult_add_su3_vector(&(P3[i]),&(P5nu[i]),coeff,&(P3[i]));
      }/* nu */

      /* Now the Lepage term... It is the same with 5-link paths with
         nu=mu and FiveSt=Lepage. So Pnumu is really Pmumu */
      u_shift_fermion(Pmu, Pnumu, OPP_DIR(mu));
      u_shift_fermion(Pnumu, P5, sig);
      if(GOES_FORWARDS(sig))
      {
        /* Add the force F_sig[x+mu+nu]:      x--+             *
         *                                   |   |             *
         *                                   o   o             *
         * the 2 link in the path: + (numbering starts form 0) */
        add_force_to_mom(P5, Pnumu, sig, Lepage) ;
      }
      /*Add the force F_nu the 1(3) link in the path: -     */
      u_shift_fermion(P5,P5nu, mu);
      side_link_force(mu, sig, -Lepage, Pmu, P5, Pnumu, P5nu) ;
      /* Add the P5nu vector to P3 */
      coeff = Lepage/ThreeSt ; 
      FORALLSITES(i,s)
        scalar_mult_add_su3_vector(&(P3[i]),&(P5nu[i]),coeff,&(P3[i]));

      /* Length 3 paths (Not the Naik term) */
      /*Add the force F_mu the 0(2) link in the path: +     */
      if(GOES_FORWARDS(mu)) 
        u_shift_fermion(P3,P3mu, mu );
      /* The above shift is not needed if mu is backwards */
      side_link_force(mu, sig, ThreeSt, temp_x, P3, Pmu, P3mu);

      /* Finally the OneLink and the Naik term */
      /* Check if this direction is not already done */
      if( (!DirectLinks[mu]) ){
        DirectLinks[mu]=1 ;
        if(GOES_BACKWARDS(mu))/* Do only the forward terms in the Dslash */
        {
          /* Because I have shifted with OPP_DIR(mu) Pmu is a forward *
           * shift.                                                   */
          /* The one link */
          add_force_to_mom(Pmu, temp_x, OPP_DIR(mu), OneLink) ;
          /* For the same reason Pnumu is the forward double link */

          /* Popmu is a backward shift */
          u_shift_fermion(temp_x, Popmu, mu);
          /* The Naik */
          /* link no 1: - */
          add_force_to_mom(Pnumu, Popmu, OPP_DIR(mu), -Naik) ;
          /*Pmumumu can overwrite Popmu which is no longer needed */
          u_shift_fermion(Pnumu, Pmumumu, OPP_DIR(mu));
          /* link no 0: + */
          add_force_to_mom(Pmumumu, temp_x, OPP_DIR(mu), Naik);
        }
        else /* The rest of the Naik terms */
        {
          u_shift_fermion(temp_x, Popmu, mu);
          /* link no 2: + */
          /* Pnumu is double backward shift */
          add_force_to_mom(Popmu, Pnumu, mu, Naik) ;
        }
      }
    }/* mu */
    /* Here we have to do together the Naik term and the one link term */
  }/*sig */

  /* Free temporary vectors */
  free(temp_x) ;
  for(mu=0;mu<8;mu++)
    free(tempvec[mu]) ;

#ifdef FFTIME
  dtime += dclock();
  node0_printf("FFTIME:  %e\n",dtime);
  /**printf("TLENGTH: %d\n",tlength);**/
#endif
} /* eo_fermion_force(version 6) */
#undef Pmu          
#undef Pnumu        
#undef Prhonumu     
#undef P7           
#undef P7rho        
#undef P7rhonu      
#undef P5           
#undef P3           
#undef P5nu         
#undef P3mu         
#undef Popmu        
#undef Pmumumu      

#define Pmu          hwvec[0] 
#define Pnumu        hwvec[1]
#define Prhonumu     hwvec[2]
#define P7           hwvec[3]
#define P7rho        hwvec[4]              
#define P7rhonu      hwvec[5]
#define P5           hwvec[6]
#define P3           hwvec[7]
#define P5nu         hwvec[3]
#define P3mu         hwvec[3]
#define Popmu        hwvec[4]
#define Pmumumu      hwvec[4]
void eo_fermion_force_3f( double eps, int nflav1, field_offset x1_off, 
    int nflav2, field_offset x2_off){
  /* note CG_solution and Dslash * solution are combined in "x_off" */
  /* New version 1/21/99.  Use forward part of Dslash to get force */
  /* 4/15/99 combine force from two different mass quarks, (eg 2+1flavors) */
  /* see long comment at end */
  /* For each link we need x_off transported from both ends of path. */
  register int i ;
  register site *s;
  int mu,nu,rho,sig ;
  int DirectLinks[8] ;
  double coeff[2],ferm_epsilon ;
  double OneLink[2], Lepage[2], Naik[2], FiveSt[2], ThreeSt[2], SevenSt[2] ;
  double mNaik[2], mLepage[2], mFiveSt[2], mThreeSt[2], mSevenSt[2] ;
  half_wilson_vector *hwvec[8] ;
  half_wilson_vector *temp_x ;

#ifdef FFTIME
  double dtime;

  dtime=-dclock();
#endif

  /* Path coefficients times fermion epsilon */
  ferm_epsilon = 2.0*(nflav1/4.0)*eps;
  OneLink[0] = act_path_coeff[0]*ferm_epsilon ;
  Naik[0]    = act_path_coeff[1]*ferm_epsilon ; mNaik[0]    = -Naik[0] ;
  ThreeSt[0] = act_path_coeff[2]*ferm_epsilon ; mThreeSt[0] = -ThreeSt[0] ;
  FiveSt[0]  = act_path_coeff[3]*ferm_epsilon ; mFiveSt[0]  = -FiveSt[0] ;
  SevenSt[0] = act_path_coeff[4]*ferm_epsilon ; mSevenSt[0] = -SevenSt[0] ;
  Lepage[0]  = act_path_coeff[5]*ferm_epsilon ; mLepage[0]  = -Lepage[0] ;

  ferm_epsilon = 2.0*(nflav2/4.0)*eps;
  OneLink[1] = act_path_coeff[0]*ferm_epsilon ;
  Naik[1]    = act_path_coeff[1]*ferm_epsilon ; mNaik[1]    = -Naik[1] ;
  ThreeSt[1] = act_path_coeff[2]*ferm_epsilon ; mThreeSt[1] = -ThreeSt[1] ;
  FiveSt[1]  = act_path_coeff[3]*ferm_epsilon ; mFiveSt[1]  = -FiveSt[1] ;
  SevenSt[1] = act_path_coeff[4]*ferm_epsilon ; mSevenSt[1] = -SevenSt[1] ;
  Lepage[1]  = act_path_coeff[5]*ferm_epsilon ; mLepage[1]  = -Lepage[1] ;
  /* *************************************** */

  /* Initialize the DirectLink flags */
  for(mu=0;mu<8;mu++)
    DirectLinks[mu] = 0 ;

  /* Allocate temporary vectors */
  for(mu=0;mu<8;mu++)
    hwvec[mu]= 
      (half_wilson_vector *)calloc(sites_on_node, sizeof(half_wilson_vector));

  /*copy x_off to a temporary vector */
  temp_x= 
    (half_wilson_vector *)calloc(sites_on_node, sizeof(half_wilson_vector));
  FORALLSITES(i,s)
  {
    temp_x[i].h[0] = *(su3_vector *)F_PT(s,x1_off) ;
    temp_x[i].h[1] = *(su3_vector *)F_PT(s,x2_off) ;
  }

  for(sig=0;sig<8;sig++)
  {
    for(mu=0;mu<8;mu++)if((mu!=sig)&&(mu!=OPP_DIR(sig)))
    {
      u_shift_hw_fermion(temp_x, Pmu, OPP_DIR(mu));
      u_shift_hw_fermion(Pmu, P3, sig);
      if(GOES_FORWARDS(sig))
      {
        /* Add the force F_sig[x+mu]:         x--+             *
         *                                   |   |             *
         *                                   o   o             *
         * the 1 link in the path: - (numbering starts form 0) */
        add_3f_force_to_mom(P3, Pmu, sig, mThreeSt) ;
      }
      for(nu=0;nu<8;nu++)if((nu!=mu )&&(nu!=OPP_DIR(mu ))&&
          (nu!=sig)&&(nu!=OPP_DIR(sig)))
      {
        u_shift_hw_fermion(Pmu, Pnumu, OPP_DIR(nu));
        u_shift_hw_fermion(Pnumu, P5, sig);
        if(GOES_FORWARDS(sig))
        {
          /* Add the force F_sig[x+mu+nu]:      x--+             *
           *                                   |   |             *
           *                                   o   o             *
           * the 2 link in the path: + (numbering starts form 0) */
          add_3f_force_to_mom(P5, Pnumu, sig, FiveSt);
        }
        for(rho=0;rho<8;rho++)if((rho!=mu )&&(rho!=OPP_DIR(mu ))&&
            (rho!=nu )&&(rho!=OPP_DIR(nu ))&&
            (rho!=sig)&&(rho!=OPP_DIR(sig)))
        {
          u_shift_hw_fermion(Pnumu, Prhonumu, OPP_DIR(rho));
          /* Length 7 paths */
          u_shift_hw_fermion(Prhonumu, P7,sig);
          if(GOES_FORWARDS(sig))
          {
            /* Add the force F_sig[x+mu+nu+rho]:  x--+             *
             *                                   |   |             *
             *                                   o   o             *
             * the 3 link in the path: - (numbering starts form 0) */
            add_3f_force_to_mom(P7, Prhonumu, sig, mSevenSt ) ;
          }
          /*Add the force F_rho the 2(4) link in the path: +     */
          u_shift_hw_fermion(P7, P7rho, rho);
          side_link_3f_force(rho,sig,SevenSt,Pnumu,P7,Prhonumu,P7rho);
          /* Add the P7rho vector to P5 */
          coeff[0] = SevenSt[0]/FiveSt[0] ; 
          coeff[1] = SevenSt[1]/FiveSt[1] ; 
          FORALLSITES(i,s)
          {
            scalar_mult_add_su3_vector(&(P5[i].h[0]),
                &(P7rho[i].h[0]),coeff[0],
                &(P5[i].h[0]));
            scalar_mult_add_su3_vector(&(P5[i].h[1]),
                &(P7rho[i].h[1]),coeff[1],
                &(P5[i].h[1]));
          }
        }/* rho */
        /* Length 5 paths */
        /*Add the force F_nu the 1(3) link in the path: -     */
        u_shift_hw_fermion(P5,P5nu, nu);
        side_link_3f_force(nu,sig,mFiveSt,Pmu,P5, 
            Pnumu,P5nu) ;
        /* Add the P5nu vector to P3 */
        coeff[0] = FiveSt[0]/ThreeSt[0] ; 
        coeff[1] = FiveSt[1]/ThreeSt[1] ; 
        FORALLSITES(i,s)
        {
          scalar_mult_add_su3_vector(&(P3[i].h[0]),
              &(P5nu[i].h[0]), coeff[0],
              &(P3[i].h[0]));
          scalar_mult_add_su3_vector(&(P3[i].h[1]),
              &(P5nu[i].h[1]), coeff[1],
              &(P3[i].h[1]));
        }
      }/* nu */

      /* Now the Lepage term... It is the same with 5-link paths with
         nu=mu and FiveSt=Lepage. So Pnumu is really Pmumu */
      u_shift_hw_fermion(Pmu, Pnumu, OPP_DIR(mu));
      u_shift_hw_fermion(Pnumu, P5, sig);
      if(GOES_FORWARDS(sig))
      {
        /* Add the force F_sig[x+mu+nu]:      x--+             *
         *                                   |   |             *
         *                                   o   o             *
         * the 2 link in the path: + (numbering starts form 0) */
        add_3f_force_to_mom(P5, Pnumu, sig, Lepage) ;
      }
      /*Add the force F_nu the 1(3) link in the path: -     */
      u_shift_hw_fermion(P5,P5nu, mu);
      side_link_3f_force(mu, sig, mLepage, Pmu, P5, Pnumu, P5nu) ;
      /* Add the P5nu vector to P3 */
      coeff[0] = Lepage[0]/ThreeSt[0] ; 
      coeff[1] = Lepage[1]/ThreeSt[1] ; 
      FORALLSITES(i,s)
      {
        scalar_mult_add_su3_vector(&(P3[i].h[0]),
            &(P5nu[i].h[0]),coeff[0],
            &(P3[i].h[0]));
        scalar_mult_add_su3_vector(&(P3[i].h[1]),
            &(P5nu[i].h[1]),coeff[1],
            &(P3[i].h[1]));
      }

      /* Length 3 paths (Not the Naik term) */
      /*Add the force F_mu the 0(2) link in the path: +     */
      if(GOES_FORWARDS(mu)) 
        u_shift_hw_fermion(P3,P3mu, mu );
      /* The above shift is not needed if mu is backwards */
      side_link_3f_force(mu, sig, ThreeSt, temp_x, P3, Pmu, P3mu);

      /* Finally the OneLink and the Naik term */
      /* Check if this direction is not already done */
      if( (!DirectLinks[mu]) ){
        DirectLinks[mu]=1 ;
        if(GOES_BACKWARDS(mu))/* Do only the forward terms in the Dslash */
        {
          /* Because I have shifted with OPP_DIR(mu) Pmu is a forward *
           * shift.                                                   */
          /* The one link */
          add_3f_force_to_mom(Pmu, temp_x, OPP_DIR(mu), OneLink) ;
          /* For the same reason Pnumu is the forward double link */

          /* Popmu is a backward shift */
          u_shift_hw_fermion(temp_x, Popmu, mu);
          /* The Naik */
          /* link no 1: - */
          add_3f_force_to_mom(Pnumu, Popmu, OPP_DIR(mu), mNaik) ;
          /*Pmumumu can overwrite Popmu which is no longer needed */
          u_shift_hw_fermion(Pnumu, Pmumumu, OPP_DIR(mu));
          /* link no 0: + */
          add_3f_force_to_mom(Pmumumu, temp_x, OPP_DIR(mu), Naik);
        }
        else /* The rest of the Naik terms */
        {
          u_shift_hw_fermion(temp_x, Popmu, mu);
          /* link no 2: + */
          /* Pnumu is double backward shift */
          add_3f_force_to_mom(Popmu, Pnumu, mu, Naik) ;
        }
      }
    }/* mu */
    /* Here we have to do together the Naik term and the one link term */
  }/*sig */

  /* Free temporary vectors */
  free(temp_x) ;
  for(mu=0;mu<8;mu++)
    free(hwvec[mu]) ;

#ifdef FFTIME
  dtime += dclock();
  node0_printf("FFTIME:  %e\n",dtime);
  /**printf("TLENGTH: %d\n",tlength);**/
#endif
} /* eo_fermion_force_3f(version 6) */
#undef Pmu          
#undef Pnumu        
#undef Prhonumu     
#undef P7           
#undef P7rho        
#undef P7rhonu      
#undef P5           
#undef P3           
#undef P5nu         
#undef P3mu         
#undef Popmu        
#undef Pmumumu      
#endif /* ASQ_OPTIMIZED_FORCE */


#ifdef  ASQ_OPTIMIZED_FATTENING   /* Asqtad action only, "_fn" executables */
#ifndef FN
BOMB THE COMPILE
#endif
void compute_gen_staple(field_offset staple, int mu, int nu, 
    field_offset link, double coef) {
  start_slice();
  su3_matrix tmat1,tmat2;
  msg_tag *mtag0,*mtag1;
  su3_matrix *tempmat ;
  register site *s ;
  register int i ;
  register su3_matrix *fat1;

  /* Computes the staple :
     mu
     +-------+
     nu	|	|
     |	|
     X	X
     Where the mu link can be any su3_matrix. The result is saved in staple.
     if staple==NULL_FP then the result is not saved.
     It also adds the computed staple to the fatlink[mu] with weight coef.
   */

  /* Upper staple */
  mtag0 = start_gather( link, sizeof(su3_matrix), nu, EVENANDODD, gen_pt[0] );
  mtag1 = start_gather( F_OFFSET(link[nu]), sizeof(su3_matrix), mu, 
      EVENANDODD, gen_pt[1] );
  wait_gather(mtag0);
  wait_gather(mtag1);

  if(staple!=NULL_FP){/* Save the staple */
    FORALLSITES(i,s){
      mult_su3_na( (su3_matrix *)gen_pt[0][i],
          (su3_matrix *)gen_pt[1][i], &tmat1 );
      mult_su3_nn( &(s->link[nu]), &tmat1, (su3_matrix *)F_PT(s,staple) );
    }
  }
  else{ /* No need to save the staple. Add it to the fatlinks */
    FORALLSITES(i,s){
      mult_su3_na( (su3_matrix *)gen_pt[0][i],
          (su3_matrix *)gen_pt[1][i], &tmat1 );
      mult_su3_nn( &(s->link[nu]), &tmat1, &tmat2 );
#ifdef DSLASH_TMP_LINKS
      fat1 = &(t_fatlink[4*i+mu]);
#else
      fat1 = &(s->fatlink[mu]);
#endif
      scalar_mult_add_su3_matrix(fat1, &tmat2, coef,
          fat1) ;
    }
  }
  cleanup_gather(mtag0);
  cleanup_gather(mtag1);

  /* lower staple */
  tempmat = (su3_matrix *)calloc(sites_on_node, sizeof(su3_matrix));
  mtag0 = start_gather( F_OFFSET(link[nu]),
      sizeof(su3_matrix), mu, EVENANDODD, gen_pt[0] );
  wait_gather(mtag0);
  FORALLSITES(i,s){
    mult_su3_an( &(s->link[nu]),(su3_matrix *)F_PT(s,link), &tmat1 );
    mult_su3_nn( &(tmat1),(su3_matrix *)gen_pt[0][i], &(tempmat[i]) );
  }
  cleanup_gather(mtag0);
  mtag0 = start_gather_from_temp( tempmat, sizeof(su3_matrix),
      OPP_DIR(nu), EVENANDODD, gen_pt[0] );
  wait_gather(mtag0);

  if(staple!=NULL_FP){/* Save the staple */
    FORALLSITES(i,s){
      add_su3_matrix( (su3_matrix *)F_PT(s,staple),(su3_matrix *)gen_pt[0][i], 
          (su3_matrix *)F_PT(s,staple) );
#ifdef DSLASH_TMP_LINKS
      fat1 = &(t_fatlink[4*i+mu]);
#else
      fat1 = &(s->fatlink[mu]);
#endif
      scalar_mult_add_su3_matrix( fat1,
          (su3_matrix *)F_PT(s,staple), coef, 
          fat1 );
    }
  }
  else{ /* No need to save the staple. Add it to the fatlinks */
    FORALLSITES(i,s){
#ifdef DSLASH_TMP_LINKS
      fat1 = &(t_fatlink[4*i+mu]);
#else
      fat1 = &(s->fatlink[mu]);
#endif
      scalar_mult_add_su3_matrix( fat1,
          (su3_matrix *)gen_pt[0][i], coef, 
          fat1 );
    }
  }

  free(tempmat);
  cleanup_gather(mtag0);
  end_slice();
}
#endif  /* ASQ_OPTIMIZED_FATTENING   */


#ifdef  ASQ_OPTIMIZED_FORCE
/*   Covariant shift of the src fermion field in the direction dir  *
 *  by one unit. The result is stored in dest.                       */ 
void u_shift_fermion(su3_vector *src, su3_vector *dest, int dir ) {
  start_slice();
  su3_vector *tmpvec ; 
  msg_tag *mtag ;
  register site *s ;
  register int i ;

  if(GOES_FORWARDS(dir)) /* forward shift */
  {
    mtag = start_gather_from_temp(src, sizeof(su3_vector), 
        dir, EVENANDODD, gen_pt[0]);
    wait_gather(mtag);
    FORALLSITES(i,s)
      mult_su3_mat_vec(&(s->link[dir]),(su3_vector *)(gen_pt[0][i]),
          &(dest[i]));
    cleanup_gather(mtag);
  }
  else /* backward shift */
  {
    tmpvec = (su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    FORALLSITES(i,s)
      mult_adj_su3_mat_vec(&(s->link[OPP_DIR(dir)]),&(src[i]), &tmpvec[i]);
    mtag = start_gather_from_temp(tmpvec, sizeof(su3_vector), dir, 
        EVENANDODD, gen_pt[0]);
    wait_gather(mtag);
    /* copy the gen_pt to the dest */
    FORALLSITES(i,s)
      dest[i] = *(su3_vector *)gen_pt[0][i];
    cleanup_gather(mtag);
    free(tmpvec) ;
  }
  end_slice();
}

/*  Covariant shift of the src half wilson fermion field in the  *
 * direction dir   by one unit. The result is stored in dest.    */
void u_shift_hw_fermion(half_wilson_vector *src, 
    half_wilson_vector *dest, int dir ) {
  half_wilson_vector *tmpvec ; 
  msg_tag *mtag ;
  register site *s ;
  register int i ;

  if(GOES_FORWARDS(dir)) /* forward shift */
  {
    mtag = start_gather_from_temp(src, sizeof(half_wilson_vector), 
        dir, EVENANDODD, gen_pt[0]);
    wait_gather(mtag);
    FORALLSITES(i,s)
      mult_su3_mat_hwvec(&(s->link[dir]),
          (half_wilson_vector *)(gen_pt[0][i]), &(dest[i]));
    cleanup_gather(mtag);
  }
  else /* backward shift */
  {
    tmpvec = 
      (half_wilson_vector *)calloc(sites_on_node, sizeof(half_wilson_vector));
    FORALLSITES(i,s)
      mult_adj_su3_mat_hwvec(&(s->link[OPP_DIR(dir)]),&(src[i]), &tmpvec[i]);
    mtag = start_gather_from_temp(tmpvec, sizeof(half_wilson_vector), dir, 
        EVENANDODD, gen_pt[0]);
    wait_gather(mtag);
    /* copy the gen_pt to the dest */
    FORALLSITES(i,s)
      dest[i] = *(half_wilson_vector *)gen_pt[0][i];
    cleanup_gather(mtag);
    free(tmpvec) ;
  }
}

/* Add in contribution to the force */
/* Put antihermitian traceless part into momentum */
void add_force_to_mom(su3_vector *back,su3_vector *forw,int dir,double coeff) {
  start_slice();
  register site *s ;
  register int i ;  
  register double tmp_coeff ;

  su3_matrix tmat,tmat2;

  if(GOES_BACKWARDS(dir))
  {
    dir = OPP_DIR(dir) ; 
    coeff = -coeff ;
  }
  FORALLSITES(i,s){
    if(s->parity==ODD) 
      tmp_coeff = -coeff ;
    else
      tmp_coeff = coeff ;
    uncompress_anti_hermitian( &(s->mom[dir]), &tmat2 );
    su3_projector(&(back[i]), &(forw[i]), &tmat);
    scalar_mult_add_su3_matrix(&tmat2, &tmat,  tmp_coeff, &tmat2 );
    make_anti_hermitian( &tmat2, &(s->mom[dir]) ); 
  }
  end_slice();
}

/* Add in contribution to the force ( 3flavor case ) */
/* Put antihermitian traceless part into momentum */
void add_3f_force_to_mom(half_wilson_vector *back,
    half_wilson_vector *forw, int dir, double coeff[2]) {
  register site *s ;
  register int i ;  
  double tmp_coeff[2] ;

  su3_matrix tmat,tmat2;

  if(GOES_BACKWARDS(dir))
  {
    dir = OPP_DIR(dir) ; 
    coeff[0] = -coeff[0] ;
    coeff[1] = -coeff[1] ;
  }
  FORALLSITES(i,s){
    if(s->parity==ODD)
    {
      tmp_coeff[0] = -coeff[0] ;
      tmp_coeff[1] = -coeff[1] ;
    }
    else
    {
      tmp_coeff[0] = coeff[0] ;
      tmp_coeff[1] = coeff[1] ;
    }
    uncompress_anti_hermitian( &(s->mom[dir]), &tmat2 );
    su3_projector(&(back[i].h[0]), &(forw[i].h[0]), &tmat);
    scalar_mult_add_su3_matrix(&tmat2, &tmat,  tmp_coeff[0], &tmat2 );
    su3_projector(&(back[i].h[1]), &(forw[i].h[1]), &tmat);
    scalar_mult_add_su3_matrix(&tmat2, &tmat,  tmp_coeff[1], &tmat2 );
    make_anti_hermitian( &tmat2, &(s->mom[dir]) ); 
  }
}

/*  This routine is needed in order to add the force on the side link *
 * of the paths in the Asq and Asqtad actions. It gets as inputs the  *
 * direction mu of the side link and the direction nu of the Dslash   *
 * term we are dealing with. Then it takes also 4 fermion fields:     *
 * Path: the piece of the path with no hop in the nu or mu direction  *
 * Path_nu: the piece of the path with a hop in nu  but not in mu     *
 * Path_mu: is Path times the link mu                                 *
 * Path_numu: is Path_nu times the link mu                            */
void side_link_force(int mu, int nu, double coeff, 
    su3_vector *Path   , su3_vector *Path_nu, 
    su3_vector *Path_mu, su3_vector *Path_numu) {
  if(GOES_FORWARDS(mu))
  {
    /*                    nu           * 
     * Add the force :  +----+         *
     *               mu |    |         *
     *                  x    (x)       *
     *                  o    o         */
    if(GOES_FORWARDS(nu))
      add_force_to_mom(Path_numu, Path, mu, coeff ) ;
    else
      add_force_to_mom(Path, Path_numu, OPP_DIR(mu), -coeff );/* ? extra - */
  }
  else /*GOES_BACKWARDS(mu)*/
  {
    /* Add the force :  o    o         *
     *               mu |    |         *
     *                  x    (x)       *
     *                  +----+         *
     *                    nu           */ 
    if(GOES_FORWARDS(nu))
      add_force_to_mom(Path_nu, Path_mu, mu, -coeff) ; /* ? extra - */
    else
      add_force_to_mom(Path_mu, Path_nu, OPP_DIR(mu), coeff) ;
  }
}

/*  The 3 flavor version of side_link_force used *
 * to optimize fermion transports                */
void side_link_3f_force(int mu, int nu, double coeff[2], 
    half_wilson_vector *Path   , 
    half_wilson_vector *Path_nu, 
    half_wilson_vector *Path_mu, 
    half_wilson_vector *Path_numu) {
  double m_coeff[2] ;

  m_coeff[0] = -coeff[0] ;
  m_coeff[1] = -coeff[1] ;

  if(GOES_FORWARDS(mu))
  {
    /*                    nu           * 
     * Add the force :  +----+         *
     *               mu |    |         *
     *                  x    (x)       *
     *                  o    o         */
    if(GOES_FORWARDS(nu))
      add_3f_force_to_mom(Path_numu, Path, mu, coeff ) ;
    else
      add_3f_force_to_mom(Path,Path_numu,OPP_DIR(mu),m_coeff);/* ? extra - */
  }
  else /*GOES_BACKWARDS(mu)*/
  {
    /* Add the force :  o    o         *
     *               mu |    |         *
     *                  x    (x)       *
     *                  +----+         *
     *                    nu           */ 
    if(GOES_FORWARDS(nu))
      add_3f_force_to_mom(Path_nu, Path_mu, mu, m_coeff) ; /* ? extra - */
    else
      add_3f_force_to_mom(Path_mu, Path_nu, OPP_DIR(mu), coeff) ;
  }
}
#endif  /* ASQ_OPTIMIZED_FORCE   */

/* LONG COMMENTS
   Here we have combined "xxx", (offset "x_off")  which is
   (M_adjoint M)^{-1} phi, with Dslash times this vector, which goes in the
   odd sites of xxx.  Recall that phi is defined only on even sites.  In
   computing the fermion force, we are looking at

   < X |  d/dt ( Dslash_eo Dslash_oe ) | X >
   =
   < X | d/dt Dslash_eo | T > + < T | d/dt Dslash_oe | X >
   where T = Dslash X.

   The subsequent manipulations to get the coefficent of H, the momentum
   matrix, in the simulation time derivative above look the same for
   the two terms, except for a minus sign at the end, if we simply stick
   T, which lives on odd sites, into the odd sites of X

   Each path in the action contributes terms when any link of the path
   is the link for which we are computing the force.  We get a minus sign
   for odd numbered links in the path, since they connect sites of the
   opposite parity from what it would be for an even numbered link.
   Minus signs from "going around" plaquette - ie KS phases, are supposed
   to be already encoded in the path coefficients.
   Minus signs from paths that go backwards are supposed to be already
   encoded in the path coefficients.

   Here, for example, are comments reproduced from the force routine for
   the one-link plus Naik plus single-staple-fat-link action:

   The three link force has three contributions, where the link that
   was differentiated is the first, second, or third link in the 3-link
   path, respectively.  Diagramatically, where "O" represents the momentum,
   the solid line the link corresponding to the momentum, and the dashed
   lines the other links:


   O______________ x ............ x ...............
   +
   x..............O______________x.................
   +
   x..............x..............O________________
   Think of this as
   < xxx | O | UUUxxx >		(  xxx, UUUX_p3 )
   +
   < xxx U | O | UUxxx >		( X_m1U , UUX_p2 )
   +
   < xxx U U | O | Uxxx >		( X_m2UU , UX_p1 )
   where "U" indicates parallel transport, "X_p3" is xxx displaced
   by +3, etc.
   Note the second contribution has a relative minus sign
   because it effectively contributes to the <odd|even>, or M_adjoint,
   part of the force when we work on an even site. i.e., for M on
   an even site, this three link path begins on an odd site.

   The staple force has six contributions from each plane containing the
   link direction:
   Call these diagrams A-F:


   x...........x		O____________x
   .			     .
   .			     .
   .			     .
   .			     .
   .			     .
   O___________x		x............x
   (A)			    (B)



   x	    x		O____________x
   .	    .		.	     .
.	    .		.	     .
.	    .		.	     .
.	    .		.	     .
.	    .		.	     .
  O___________x		x	     x
(C)			    (D)



  x...........x		O____________x
  .			.
  .			.
  .			.
  .			.
  .			.
  O___________x		x............x
(E)			    (F)

  As with the Naik term, diagrams C and D have a relative minus
  sign because they connect sites of the other parity.

  Also note an overall minus sign in the staple terms relative to the
  one link term because, with the KS phase factors included, the fat
  link is  "U - w3 * UUU", or the straight link MINUS w3 times the staples.

  Finally, diagrams B and E get one more minus sign because the link
  we are differentiating is in the opposite direction from the staple
  as a whole.  You can think of this as this "U" being a correction to
  a "U_adjoint", but the derivative of U is iHU and the derivative
  of U_adjoint is -iHU_adjoint.

  */
  /* LONG COMMENT on sign conventions
     In most of the program, the KS phases and antiperiodic boundary
     conditions are absorbed into the link matrices.  This greatly simplfies
     multiplying by the fermion matrix.  However, it requires care in
     specifying the path coefficients.  Remember that each time you
     encircle a plaquette, you pick up a net minus sign from the KS phases.
     Thus, when you have more than one path to the same point, you generally
     have a relative minus sign for each plaquette in a surface bounded by
     this path and the basic path for that displacement.

     Examples:
     Fat Link:
     Positive:	X-------X

     Negative     --------
     |	|
     |	|
     X	X

     Naik connection, smeared
     Positive:	X-------x-------x-------X

     Negative:	---------
     |	|
     |	|
     X	x-------x-------X

     Positive:	--------x--------
     |		|
     |		|
     X		x-------X

     Negative:	--------x-------x-------x
     |			|
     |			|
     X			X
   */



  /* Comment on acceptable actions.
     We construct the backwards part of dslash by reversing all the
     paths in the forwards part.  So, for example, in the p4 action
     the forwards part includes +X+Y+Y

     X
     |
     |
     X
     |
     |
     X---->--X

     so we put -X-Y-Y in the backwards part.  But this isn't the adjoint
     of U_x(0)U_y(+x)U_y(+x+y).  Since much of the code assumes that the
     backwards hop is the adjoint of the forwards (for example, in
     preventing going to 8 flavors), the code only works for actions
     where this is true.  Roughly, this means that the fat link must
     be symmetric about reflection around its midpoint.  Equivalently,
     the paths in the backwards part of Dslash are translations of the
     paths in the forwards part.  In the case of the "P4" or knight's move
     action, this means that we have to have both paths
     +X+Y+Y and +Y+Y+X to the same point, with the same coefficients.
     Alternatively, we could just use the symmetric path +Y+X+Y.
   */
