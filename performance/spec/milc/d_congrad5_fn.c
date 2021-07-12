/******* d_congrad5_fn_tmp.c - conjugate gradient for SU3/fermions ****/
/* MIMD version 6 */
/* Kogut-Susskind fermions -- this version for "fat plus Naik" quark
   actions.  

   This code combines d_congrad5_fn.c and d_congrad5_fn_tmp.c
   With CONGRAD_TMP_VECTORS defined, allocates temporary CG vectors in
   field-major order and uses them instead of the site-major
   temporaries.  They may be eliminated from the site structure in the
   future.

   Calls dslash_fn or dslash_fn_on_temp depending accordingly. */

/* Jim Hetrick, Kari Rummukainen, Doug Toussaint, Steven Gottlieb */
/* 10/02/01 C. DeTar Consolidated with tmp version */

/* This version looks at the initial vector every "niter" passes */
/* The source vector is in "src", and the initial guess and answer
   in "dest".  "resid" is the residual vector, and "cg_p" and "ttt" are
   working vectors for the conjugate gradient.
   niter = maximum number of iterations.
   rsqmin = desired rsq, quit when we reach rsq <= rsqmin*source_norm.
   This is different than our old definition of the stopping
   criterion.  To convert an old stopping residual to the new
   one, multiply the old one by sqrt( (2/3)/(8+2*m) )
   This is because the source is obtained from
   a random vector with average squared magnitude 3 on each site.
   Then, on 1/2 the sites, we gather and sum the eight neighboring
   random vectors and add 2*m times the local vector.
   source = M_adjoint*R, on even sites
   reinitialize after niters iterations and try once more.
   parity=EVEN = do only even sites, parity=ODD = do odd sites,
   parity=EVENANDODD = do all sites
 */
#include "generic_ks_includes.h"	/* definitions files and prototypes */
#define FETCH_UP 1
#ifdef HAVE_SYS_TIME_H 
#include <sys/time.h> 
#endif 

static void test_slice(){
  __asm__ __volatile__ ("");
}
static void start_slice(){
  __asm__ __volatile__ ("");
}
static void end_slice(){
  __asm__ __volatile__ ("");
}

void cleanup_gathers(msg_tag *t1[16],msg_tag *t2[16]); /* dslash_fn_tmp.c */

#define LOOPEND
#include "loopend.h"

#ifdef CONGRAD_TMP_VECTORS
su3_vector *ttt,*cg_p;
su3_vector *resid;
su3_vector *t_dest;
int first_congrad = 1;
#endif

int ks_congrad( field_offset src, field_offset dest, double mass,
    int niter, double rsqmin, int parity, double *final_rsq_ptr ){
  register int i;
  register site *s;
  int iteration;	/* counter for iterations */
  double a,b;			/* Sugar's a,b,resid**2,last resid*2 */
  double rsq,oldrsq,pkp;		/* pkp = cg_p.K.cg_p */
  double msq_x4;	/* 4*mass*mass */
  double source_norm;	/* squared magnitude of source vector */
  double rsqstop;	/* stopping residual normalized by source norm */
  int l_parity;	/* parity we are currently doing */
  int l_otherparity;	/* the other parity */
  msg_tag * tags1[16], *tags2[16];	/* tags for gathers to parity and opposite */
  int special_started;	/* 1 if dslash_special has been called */

  /* Timing */
#ifdef HAVE_SYS_TIME_H
  struct timeval tv1c, tv2c, tv1d, tv2d;	
  double dt1, dt2;			/* time for one iter */
#endif

#ifdef CGTIME
  double dtimed,dtimec;
#endif
  double nflop;

  /* debug */
#ifdef CGTIME
  dtimec = -dclock(); 
#ifdef HAVE_SYS_TIME_H
  gettimeofday(&tv1c, (struct timeval*)0);      
#endif
#endif

  nflop = 1187;
  if(parity==EVENANDODD)nflop *=2;

  special_started=0;
  /* if we want both parities, we will do even first. */
  switch(parity){
    case(EVEN): l_parity=EVEN; l_otherparity=ODD; break;
    case(ODD):  l_parity=ODD; l_otherparity=EVEN; break;
    case(EVENANDODD):  l_parity=EVEN; l_otherparity=ODD; break;
  }
  msq_x4 = 4.0*mass*mass;
  iteration = 0;

  if (!valid_longlinks) load_longlinks();
  if (!valid_fatlinks) load_fatlinks();
#ifdef CONGRAD_TMP_VECTORS
  /* now we can allocate temporary variables and copy then */
  /* PAD may be used to avoid cache trashing */
#define PAD 0

  if(first_congrad) {
    ttt = (su3_vector *) calloc((sites_on_node+PAD), sizeof(su3_vector));
    cg_p = (su3_vector *) calloc((sites_on_node+PAD), sizeof(su3_vector));
    resid = (su3_vector *) calloc((sites_on_node+PAD), sizeof(su3_vector));
    t_dest = (su3_vector *) calloc((sites_on_node+PAD), sizeof(su3_vector));
    first_congrad = 0;
  }
#endif

#ifdef CGTIME
  dtimec = -dclock(); 
#endif

#ifdef CONGRAD_TMP_VECTORS
  /* now we copy dest to temporaries */
  FORALLSITES(i,s) {
    t_dest[i] = *(su3_vector *)F_PT(s,dest);
  }
#endif

  /* initialization process */
start:
  /**node0_printf("ks_congrad4: start, parity = %d\n",parity);**/
  /* ttt <-  (-1)*M_adjoint*M*dest
     resid,cg_p <- src + ttt
     rsq = |resid|^2
     source_norm = |src|^2
   */
  if(special_started==1) {	/* clean up gathers */
    cleanup_gathers(tags1,tags2);
    special_started=0;
  }
  /**if(this_node==0)if(iteration>1)printf("CONGRAD: restart rsq = %.10e\n",rsq);**/
  rsq = source_norm = 0.0;
#ifdef CONGRAD_TMP_VECTORS
  dslash_fn_on_temp_special(t_dest, ttt,l_otherparity,tags2,1);
  dslash_fn_on_temp_special(ttt,ttt,l_parity,tags1,1);
  cleanup_gathers(tags1,tags2);
#else
  /* Why not use dslash_fn_special here ??? -CD */
  dslash_fn( dest, F_OFFSET(ttt), l_otherparity);
  dslash_fn(F_OFFSET(ttt),F_OFFSET(ttt),l_parity);
#endif
  /* ttt  <- ttt - msq_x4*src	(msq = mass squared) */
  FORSOMEPARITY(i,s,l_parity){
#ifdef CONGRAD_TMP_VECTORS
    scalar_mult_add_su3_vector( &ttt[i], &t_dest[i],
        -msq_x4, &ttt[i] );
    /* note that we go back to the site structure for src */
    add_su3_vector( (su3_vector *)F_PT(s,src),
        &ttt[i], &resid[i] );
    /* remember ttt contains -M_adjoint*M*src */
    cg_p[i] = resid[i];
    /* note that we go back to the site structure for src */
    source_norm += (double)magsq_su3vec( (su3_vector *)F_PT(s,src) );
    rsq += (double)magsq_su3vec( &resid[i] );
#else
    scalar_mult_add_su3_vector( &(s->ttt), (su3_vector *)F_PT(s,dest),
        -msq_x4, &(s->ttt) );
    add_su3_vector( (su3_vector *)F_PT(s,src),
        &(s->ttt), &(s->resid) );
    s->cg_p = s->resid;
    source_norm += (double) magsq_su3vec( (su3_vector *)F_PT(s,src) );
    rsq += (double) magsq_su3vec( &(s->resid) );
#endif
  } END_LOOP
  g_doublesum( &source_norm );
  g_doublesum( &rsq );
  /**if(this_node==0)printf("CONGRAD: start rsq = %.10e\n",rsq);**/
  iteration++ ;  /* iteration counts number of multiplications
                    by M_adjoint*M */
  total_iters++;
  rsqstop = rsqmin * source_norm;
  /**node0_printf("congrad: source_norm = %e\n", (double)source_norm);**/
  if( rsq <= rsqstop ){
    /* if parity==EVENANDODD, set up to do odd sites and go back */
    if(parity == EVENANDODD) {
      l_parity=ODD; l_otherparity=EVEN;
      parity=EVEN;	/* so we won't loop endlessly */
      iteration = 0;
      /**node0_printf("instant goto start\n"); **/
      goto start;
    }
    *final_rsq_ptr=(double)rsq;
    /**node0_printf("instant return\n"); fflush(stdout);**/
    return (iteration);
  }
  /**pkp=0.0;
    if(mynode()==0){printf("iter=%d, rsq= %e, pkp=%e\n",
    iteration,(double)rsq,(double)pkp);fflush(stdout);}**/

  /* main loop - do until convergence or time to restart */
  /*
     oldrsq <- rsq
     ttt <- (-1)*M_adjoint*M*cg_p
     pkp <- (-1)*cg_p.M_adjoint*M.cg_p
     a <- -rsq/pkp
     dest <- dest + a*cg_p
     resid <- resid + a*ttt
     rsq <- |resid|^2
     b <- rsq/oldrsq
     cg_p <- resid + b*cg_p
   */
  do{
    oldrsq = rsq;
    pkp = 0.0;
    /* sum of neighbors */

    if(special_started==0){
#ifdef CONGRAD_TMP_VECTORS
      dslash_fn_on_temp_special( cg_p, ttt, l_otherparity, tags2, 1 );
      dslash_fn_on_temp_special( ttt, ttt, l_parity, tags1, 1);
#else
      dslash_fn_special( F_OFFSET(cg_p), F_OFFSET(ttt), l_otherparity,
          tags2, 1 );
      dslash_fn_special( F_OFFSET(ttt), F_OFFSET(ttt), l_parity,
          tags1, 1);
#endif
      special_started=1;
    }
    else {
#ifdef CONGRAD_TMP_VECTORS
      dslash_fn_on_temp_special( cg_p, ttt, l_otherparity, tags2, 0 );
      dslash_fn_on_temp_special( ttt, ttt, l_parity, tags1, 0);
#else
      dslash_fn_special( F_OFFSET(cg_p), F_OFFSET(ttt), l_otherparity,
          tags2, 0 );
      dslash_fn_special( F_OFFSET(ttt), F_OFFSET(ttt), l_parity,
          tags1, 0 );
#endif
    }

    /* finish computation of M_adjoint*m*p and p*M_adjoint*m*Kp */
    /* ttt  <- ttt - msq_x4*cg_p	(msq = mass squared) */
    /* pkp  <- cg_p.(ttt - msq*cg_p) */
    pkp = 0.0;
    FORSOMEPARITY(i,s,l_parity){
#ifdef CONGRAD_TMP_VECTORS
      scalar_mult_add_su3_vector( &ttt[i], &cg_p[i], -msq_x4,
          &ttt[i] );
      pkp += (double)su3_rdot( &cg_p[i], &ttt[i] );
#else
      scalar_mult_add_su3_vector( &(s->ttt), &(s->cg_p), -msq_x4,
          &(s->ttt) );
      pkp += (double)su3_rdot( &(s->cg_p), &(s->ttt) );
#endif
    } END_LOOP
    g_doublesum( &pkp );
    iteration++;
    total_iters++;

    a = (double) (-rsq/pkp);

    /* dest <- dest - a*cg_p */
    /* resid <- resid - a*ttt */
    rsq=0.0;
    FORSOMEPARITY(i,s,l_parity){
#ifdef CONGRAD_TMP_VECTORS
      scalar_mult_add_su3_vector( &t_dest[i], &cg_p[i], a, &t_dest[i] );
      scalar_mult_add_su3_vector( &resid[i], &ttt[i], a, &resid[i]);
      rsq += (double)magsq_su3vec( &resid[i] );
#else
      scalar_mult_add_su3_vector( (su3_vector *)F_PT(s,dest),
          &(s->cg_p), a, (su3_vector *)F_PT(s,dest) );
      scalar_mult_add_su3_vector( &(s->resid), &(s->ttt), a, &(s->resid));
      rsq += (double)magsq_su3vec( &(s->resid) );
#endif
    } END_LOOP
    g_doublesum(&rsq);
    /**if(mynode()==0){printf("iter=%d, rsq= %e, pkp=%e\n",
      iteration,(double)rsq,(double)pkp);fflush(stdout);}**/

    if( rsq <= rsqstop ){
#ifdef CONGRAD_TMP_VECTORS
      /* copy t_dest back to site structure */
      FORSOMEPARITY(i,s,l_parity){
        *(su3_vector *)F_PT(s,dest) = t_dest[i];
      } END_LOOP
#endif
      /* if parity==EVENANDODD, set up to do odd sites and go back */
      if(parity == EVENANDODD) {
        l_parity=ODD; l_otherparity=EVEN;
        parity=EVEN;	/* so we won't loop endlessly */
        iteration = 0;
        /**node0_printf("normal goto start\n"); **/
        goto start;
      }
      *final_rsq_ptr=(double)rsq + 0.0f;
      if(special_started==1) {
        cleanup_gathers(tags1,tags2);
        special_started = 0;
      }

      /**node0_printf("normal return\n"); fflush(stdout);**/
#ifdef CGTIME
      dtimec += dclock();
#ifdef HAVE_SYS_TIME_H
      gettimeofday(&tv2c, (struct timeval*)0);
      dt1 = (tv2c.tv_sec - tv1c.tv_sec) + (tv2c.tv_usec - tv1c.tv_usec)/1.e6;
      if(this_node==0)printf("CONGRADwall time = %e iters = %d mflops = %e\n",dt1,iteration,(double)(nflop*volume*iteration/(1.0e6*dt1*numnodes())) );
#endif
      if(this_node==0){printf("CONGRAD5: time = %e iters = %d mflops = %e\n",
          dtimec,iteration,(double)(nflop*volume*iteration/(1.0e6*dtimec*numnodes())) );
      fflush(stdout);}
#endif
      return (iteration);
    }

    b = (double)rsq/oldrsq;
    /* cg_p  <- resid + b*cg_p */
#ifdef CONGRAD_TMP_VECTORS
    FORSOMEPARITY(i,s,l_parity){
      scalar_mult_add_su3_vector( &resid[i],
          &cg_p[i] , b , &cg_p[i]);
    } END_LOOP
#else
    scalar_mult_add_latvec( F_OFFSET(resid), F_OFFSET(cg_p),
        b, F_OFFSET(cg_p), l_parity);
#endif

  } while( iteration%niter != 0);

      if( iteration < 5*niter ){
        /**node0_printf("try again goto start\n");**/
        goto start;
      }
#ifdef CONGRAD_TMP_VECTORS
      /* if we have gotten here, no convergence after several restarts: must
         copy t_dest back to site structure */
      FORSOMEPARITY(i,s,l_parity){
        *(su3_vector *)F_PT(s,dest) = t_dest[i];
      } END_LOOP
#endif

      /* if parity==EVENANDODD, set up to do odd sites and go back */
      if(parity == EVENANDODD) {
        l_parity=ODD; l_otherparity=EVEN;
        parity=EVEN;	/* so we won't loop endlessly */
        iteration = 0;
        goto start;
      }

      *final_rsq_ptr=rsq + 0.0f;
      if(special_started==1){	/* clean up gathers */
        cleanup_gathers(tags1,tags2);
        special_started = 0;
      }
      node0_printf(
          "CG not converged after %d iterations, res. = %e wanted %e\n",
          iteration,rsq,rsqstop);
      fflush(stdout);
      return(iteration);
}

/* clear an su3_vector in the lattice */
void clear_latvec(field_offset v,int parity){
  register int i,j;
  register site *s;
  register su3_vector *vv;
  switch(parity){
    case EVEN: FOREVENSITES(i,s){
                 vv = (su3_vector *)F_PT(s,v);
                 for(j=0;j<3;j++){ vv->c[j].real = vv->c[j].imag = 0.0; }
               } break;
    case ODD: FORODDSITES(i,s){
                vv = (su3_vector *)F_PT(s,v);
                for(j=0;j<3;j++){ vv->c[j].real = vv->c[j].imag = 0.0; }
              } break;
    case EVENANDODD: FORALLSITES(i,s){
                       vv = (su3_vector *)F_PT(s,v);
                       for(j=0;j<3;j++){ vv->c[j].real = vv->c[j].imag = 0.0; }
                     } break;
  } 
}

/* copy an su3_vector in the lattice */
void copy_latvec(field_offset src,field_offset dest,int parity){
  register int i;
  register site *s;
  register su3_vector *spt,*dpt;
  switch(parity){
    case EVEN: FOREVENSITES(i,s){
                 s = &(lattice[i]);
                 spt = (su3_vector *)F_PT(s,src);
                 dpt = (su3_vector *)F_PT(s,dest);
                 *dpt = *spt;
               } break;
    case ODD: FORODDSITES(i,s){
                s = &(lattice[i]);
                spt = (su3_vector *)F_PT(s,src);
                dpt = (su3_vector *)F_PT(s,dest);
                *dpt = *spt;
              } break;
    case EVENANDODD: FORALLSITES(i,s){
                       s = &(lattice[i]);
                       spt = (su3_vector *)F_PT(s,src);
                       dpt = (su3_vector *)F_PT(s,dest);
                       *dpt = *spt;
                     } break;
  } 
}

/* scalar multiply and add an SU3 vector in the lattice */
void scalar_mult_add_latvec(field_offset src1,field_offset src2,
    double scalar,field_offset dest,int parity)
{
  start_slice();
  register int i;
  register site *s;
  register su3_vector *spt1,*spt2,*dpt;
  FORSOMEPARITY(i,s,parity){
    spt1 = (su3_vector *)F_PT(s,src1);
    spt2 = (su3_vector *)F_PT(s,src2);
    dpt = (su3_vector *)F_PT(s,dest);
    scalar_mult_add_su3_vector( spt1 , spt2 , scalar , dpt);
  } END_LOOP
  end_slice();
}


void scalar2_mult_add_su3_vector(su3_vector *a, double s1, su3_vector *b, 
    double s2, su3_vector *c){
  register int i;
  for(i=0;i<3;i++){
    c->c[i].real = s1*a->c[i].real + s2*b->c[i].real;
    c->c[i].imag = s1*a->c[i].imag + s2*b->c[i].imag;
  }
}

/* scalar multiply two SU3 vector and add through the lattice */
void scalar2_mult_add_latvec(field_offset src1,double scalar1,
    field_offset src2,double scalar2,
    field_offset dest,int parity)
{
  register int i;
  register site *s;
  register su3_vector *spt1,*spt2,*dpt;
  FORSOMEPARITY(i,s,parity){
    spt1 = (su3_vector *)F_PT(s,src1);
    spt2 = (su3_vector *)F_PT(s,src2);
    dpt  = (su3_vector *)F_PT(s,dest);
    scalar2_mult_add_su3_vector( spt1, scalar1, spt2, scalar2, dpt);
  } END_LOOP
}

/* scalar multiply an SU3 vector in the lattice */
void scalar_mult_latvec(field_offset src,double scalar,
    field_offset dest,int parity)
{
  start_slice();
  register int i;
  register site *s;
  register su3_vector *spt,*dpt;
  switch(parity){
    case EVEN: FOREVENSITES(i,s){
                 spt = (su3_vector *)F_PT(s,src);
                 dpt = (su3_vector *)F_PT(s,dest);
                 scalar_mult_su3_vector( spt , scalar , dpt );
               } break;
    case ODD: FORODDSITES(i,s){
                spt = (su3_vector *)F_PT(s,src);
                dpt = (su3_vector *)F_PT(s,dest);
                scalar_mult_su3_vector( spt , scalar , dpt );
              } break;
    case EVENANDODD: FORALLSITES(i,s){
                       spt = (su3_vector *)F_PT(s,src);
                       dpt = (su3_vector *)F_PT(s,dest);
                       scalar_mult_su3_vector( spt , scalar , dpt );
                     } break;
  } 
  end_slice();
}

