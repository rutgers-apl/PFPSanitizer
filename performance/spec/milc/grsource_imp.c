/************************ grsource_imp.c *****************************/
/* MIMD version 6 */
/* Kogut-Susskind fermions  -- this version for "fat plus Naik"
   or general "even plus odd" quark actions.  Assumes "dslash" has
   been defined to be the appropriate "dslash_fn" or "dslash_eo"
*/
#include "generic_ks_includes.h"	/* definitions files and prototypes */

#ifdef FN
#define dslash dslash_fn
#endif
#ifdef EO
#define dslash dslash_eo
#endif

/* construct a gaussian random vector, g_rand, and phi=M(dagger)*g_rand  */
/* "parity" is EVEN, ODD, or EVENANDODD.  The parity is the parity at
    which phi is computed.  g_rand must always be computed at all sites. */

void grsource_imp( field_offset dest, double mass, int parity) {
register int i,j;
register site *s;
    FORALLSITES(i,s){
        for(j=0;j<3;j++){
#ifdef SITERAND
            s->g_rand.c[j].real = gaussian_rand_no(&(s->site_prn));
            s->g_rand.c[j].imag = gaussian_rand_no(&(s->site_prn));
#else
            s->g_rand.c[j].real = gaussian_rand_no(&node_prn);
            s->g_rand.c[j].imag = gaussian_rand_no(&node_prn);
#endif
        }
    }
#ifdef FN
    if( !valid_longlinks )load_longlinks();
    if( !valid_fatlinks )load_fatlinks();
#endif
    dslash( F_OFFSET(g_rand), dest, parity);
    scalar_mult_latvec( dest, -1.0, dest, parity );
    scalar_mult_add_latvec( dest, F_OFFSET(g_rand), 2.0*mass,
	dest, parity );
}/* grsource */


/* Check congrad by multiplying src by M, compare result to g_rand */
/* Before calling checkmul() you should call grsource(EVENANDODD) and
   congrad(...,EVENANDODD) */
void checkmul_imp( field_offset src, double mass ) {
register int i,j;
register site *s;
    dslash( src, F_OFFSET(ttt), EVENANDODD);
    scalar_mult_add_latvec( F_OFFSET(ttt), src, 2.0*mass,
	F_OFFSET(ttt), EVENANDODD );
    FORALLSITES(i,s){
	printf("Site %d %d %d %d\n",s->x,s->y,s->z,s->t);
	for(j=0;j<3;j++){
	    printf("%d %d\t%e\t%e\t%e\n",i,j,(double)s->g_rand.c[j].real,
		(double)s->ttt.c[j].real,(double)s->g_rand.c[j].real - (double)s->ttt.c[j].real);
	    printf("%d %d\t%e\t%e\t%e\n",i,j,(double)s->g_rand.c[j].imag,
		(double)s->ttt.c[j].imag,(double)s->g_rand.c[j].imag - (double)s->ttt.c[j].imag);
	}
	printf("\n");
/**sleep(2);**/
    }
}/* checkmul */
