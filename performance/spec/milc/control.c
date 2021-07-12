/************************* control.c *******************************/
/* MIMD version 6 */
/* Main procedure for SU3 with dynamical fermions 			*/
/* Naik plus fat link fermions, general gauge action */

/* This version combines code for the PHI algorithm (approriate for 4
   flavors) and the R algorithm for "epsilon squared" updating of 
   1 to 4 flavors.  Compilation should occur with PHI_ALGORITHM defined
   for the former and not defined for the latter.  It also contains code
   for the hybrid Monte Carlo algorithm, for which HMC_ALGORITHM and
   PHI_ALGORITHM should be defined.  (Actually, the
   changes to control.c are minimal and the real differences will appear
   in update.c */

#define CONTROL
#include "ks_imp_includes.h"	/* definitions files and prototypes */

EXTERN  gauge_header start_lat_hdr;     /* Input gauge field header */

int main( int argc, char **argv ){
    int meascount,traj_done;
    int prompt;
    int s_iters,avs_iters,avspect_iters,
        avbcorr_iters;
    double dtime, dclock();

    initialize_machine(argc,argv);
    g_sync();
    /* set up */
    prompt = setup();
    /* loop over input sets */
    while( readin(prompt) == 0){

      /* perform warmup trajectories */
      dtime = -dclock();
      for( traj_done=0; traj_done < warms; traj_done++ ){
        update();
      }
      node0_printf("WARMUPS COMPLETED\n"); fflush(stdout);

      /* perform measuring trajectories, reunitarizing and measuring 	*/
      meascount=0;		/* number of measurements 		*/
      avspect_iters = avs_iters = avbcorr_iters = 0;
      for( traj_done=0; traj_done < trajecs; traj_done++ ){ 

        /* do the trajectories */
        s_iters=update();

        /* measure every "propinterval" trajectories */
        if( (traj_done%propinterval)==(propinterval-1) ){

          /* call gauge_variable fermion_variable measuring routines */
          /* results are printed in output file */
          rephase(OFF);
          g_measure( );
          rephase(ON);
          f_meas_imp(F_OFFSET(phi),F_OFFSET(xxx),mass);
#ifdef SPECTRUM 
          rephase( OFF );
          gaugefix(TUP,(double)1.8,500,(double)GAUGE_FIX_TOL,
              F_OFFSET(tempmat1),F_OFFSET(tempvec[0]),
              0,NULL,NULL,0,NULL,NULL);
          rephase( ON );
#ifdef FN
          valid_fatlinks = valid_longlinks = 0;
#endif
          avspect_iters += spectrum2(mass,F_OFFSET(phi),F_OFFSET(xxx));

          /* Fix TUP Coulomb gauge - gauge links only*/
          rephase( OFF );
          gaugefix(TUP,(double)1.8,500,(double)GAUGE_FIX_TOL,
              F_OFFSET(tempmat1),F_OFFSET(staple),
              0,NULL,NULL,0,NULL,NULL);
          rephase( ON );
          valid_fatlinks = valid_longlinks = 0;

          /* Comment these spectrum calls out as you wish */
          /* There are no side-effects */
          avspect_iters += nl_spectrum(mass,F_OFFSET(phi),F_OFFSET(xxx),
              F_OFFSET(tempmat1),F_OFFSET(staple));
          avspect_iters += spectrum_mom(mass,mass,F_OFFSET(phi),5e-3);
          avspect_iters += spectrum_multimom(mass,mass,mass/4.,4,5e-3);
          avspect_iters += spectrum_nlpi2(mass,mass,F_OFFSET(phi),5e-3);
#ifdef FN
          if( !valid_fatlinks )load_fatlinks();
#endif
          rephase( OFF );
          hvy_pot( F_OFFSET(link[XUP]) );
          rephase( ON );
#endif
          avs_iters += s_iters;
          ++meascount;
          fflush(stdout);
        }
      }	/* end loop over trajectories */

      node0_printf("RUNNING COMPLETED\n"); fflush(stdout);
      if(meascount>0)  {
        node0_printf("average cg iters for step= %e\n",
            (double)avs_iters/meascount);
#ifdef SPECTRUM
        node0_printf("average cg iters for spectrum = %e\n",
            (double)avspect_iters/meascount);
#endif
      }

      dtime += dclock();
      if(this_node==0){
        /*CPU2006  printf("Time = %e seconds\n",dtime);*/
        printf("total_iters = %d\n",total_iters);
      }
      fflush(stdout);

      /* save lattice if requested */
      if( saveflag != FORGET ){
        rephase( OFF );
        save_lattice( saveflag, savefile );
        rephase( ON );
      }
    }
    return 0;
}
