/* benchmark specific
*  since the trajectory can deviate because of
*  floating point errors we need to be able
* to check for the significance of the deviation
*
* i.e. would we be upset with this variation
*      in a real world run
*
* this is not hard to quantify, but is hard to set up an
* automatic pearl level test
*
* so we add it to AMMP
*/
/* check the values for numstp, delta and rmsdev
* these are set first in the script
*  (if they arent then every run is significant)
*/

#include <stdio.h>
#include <stdlib.h>
#include "ammp.h"
float get_f_variable(char *name );

int significance(op)
FILE *op;
{
	int get_i_variable();
	int ns,na,a_number();
	float delta,rmsd;

	ns = get_i_variable("numstp");
	na = a_number();
	delta = get_f_variable("delta");
	rmsd = get_f_variable("rmsdev");

	if( ns <= 0 )
	{ fprintf(op," you are cheating, seti numstp <number of MD steps>\n");
	}
	
	if( delta > 20. || delta < -20.)
	{
	fprintf( op," The drift in the total energy is too high\n");	
	return 0;
	}
	if( ns < 100 && rmsd > 0.01 )
	{
	fprintf( op," The RMSD is too high \n");
	return 0;
	}
	if( ns < 1000 && ns > 99 && rmsd > 0.1 )
	{
	fprintf( op," The RMSD is too high \n");
	return 0;
	}
	if( ns < 10000 && ns > 999 && rmsd > 1.0 )
	{
	fprintf( op," The RMSD is too high \n");
	return 0;
	}

	fprintf(op," The run is ok\n");

	return 0;
}/* end of routine */
