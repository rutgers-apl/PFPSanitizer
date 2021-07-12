#ifndef _QLA_DF_H
#define _QLA_DF_H

/***************************************************************/
/* Level 1 Linear Algebra API QLA precision DF color  */
/***************************************************************/


/*===================================================*/
/* Type conversion and component extraction */
/*===================================================*/


/*---------------------------------------------------*/
/* Convert float to double */
/*---------------------------------------------------*/

void QLA_DF_R_eq_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a);
void QLA_DF_R_veq_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int n);
void QLA_DF_R_xeq_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int *index, int n);
void QLA_DF_R_veq_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int n);
void QLA_DF_R_xeq_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int *index, int n);
void QLA_DF_C_eq_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a);
void QLA_DF_C_veq_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int n);
void QLA_DF_C_xeq_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int *index, int n);
void QLA_DF_C_veq_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int n);
void QLA_DF_C_xeq_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int *index, int n);

/*---------------------------------------------------*/
/* Convert double to float */
/*---------------------------------------------------*/

void QLA_FD_R_eq_R ( QLA_F_Real *QLA_RESTRICT r, QLA_D_Real *QLA_RESTRICT a);
void QLA_FD_R_veq_R ( QLA_F_Real *QLA_RESTRICT r, QLA_D_Real *QLA_RESTRICT a, int n);
void QLA_FD_R_xeq_R ( QLA_F_Real *QLA_RESTRICT r, QLA_D_Real *QLA_RESTRICT a, int *index, int n);
void QLA_FD_R_veq_pR ( QLA_F_Real *QLA_RESTRICT r, QLA_D_Real *QLA_RESTRICT *a, int n);
void QLA_FD_R_xeq_pR ( QLA_F_Real *QLA_RESTRICT r, QLA_D_Real *QLA_RESTRICT *a, int *index, int n);
void QLA_FD_C_eq_C ( QLA_F_Complex *QLA_RESTRICT r, QLA_D_Complex *QLA_RESTRICT a);
void QLA_FD_C_veq_C ( QLA_F_Complex *QLA_RESTRICT r, QLA_D_Complex *QLA_RESTRICT a, int n);
void QLA_FD_C_xeq_C ( QLA_F_Complex *QLA_RESTRICT r, QLA_D_Complex *QLA_RESTRICT a, int *index, int n);
void QLA_FD_C_veq_pC ( QLA_F_Complex *QLA_RESTRICT r, QLA_D_Complex *QLA_RESTRICT *a, int n);
void QLA_FD_C_xeq_pC ( QLA_F_Complex *QLA_RESTRICT r, QLA_D_Complex *QLA_RESTRICT *a, int *index, int n);

/*===================================================*/
/* Reductions               */
/*===================================================*/


/*---------------------------------------------------*/
/* Global squared norm float to double precision */
/*---------------------------------------------------*/


/*  QLA_Real (r = sum norm2 a) */


void QLA_DF_r_eq_norm2_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a);
void QLA_DF_r_veq_norm2_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int n);
void QLA_DF_r_xeq_norm2_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int *index, int n);
void QLA_DF_r_veq_norm2_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int n);
void QLA_DF_r_xeq_norm2_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int *index, int n);

/*  QLA_Complex (r = sum norm2 a) */


void QLA_DF_r_eq_norm2_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a);
void QLA_DF_r_veq_norm2_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int n);
void QLA_DF_r_xeq_norm2_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int *index, int n);
void QLA_DF_r_veq_norm2_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int n);
void QLA_DF_r_xeq_norm2_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int *index, int n);

/*---------------------------------------------------*/
/* Global inner product float to double */
/*---------------------------------------------------*/


/*  QLA_Real (c = trace adj(a) dot b) */


void QLA_DF_r_eq_R_dot_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, QLA_F_Real *QLA_RESTRICT b);
void QLA_DF_r_veq_R_dot_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, QLA_F_Real *QLA_RESTRICT b, int n);
void QLA_DF_r_xeq_R_dot_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, QLA_F_Real *QLA_RESTRICT b, int *index, int n);
void QLA_DF_r_veq_pR_dot_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, QLA_F_Real *QLA_RESTRICT b, int n);
void QLA_DF_r_veq_R_dot_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, QLA_F_Real *QLA_RESTRICT *b, int n);
void QLA_DF_r_veq_pR_dot_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, QLA_F_Real *QLA_RESTRICT *b, int n);
void QLA_DF_r_xeq_pR_dot_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, QLA_F_Real *QLA_RESTRICT b, int *index, int n);
void QLA_DF_r_xeq_R_dot_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, QLA_F_Real *QLA_RESTRICT *b, int *index, int n);
void QLA_DF_r_xeq_pR_dot_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, QLA_F_Real *QLA_RESTRICT *b, int *index, int n);

/*  QLA_Complex (c = trace adj(a) dot b) */


void QLA_DF_c_eq_C_dot_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b);
void QLA_DF_c_veq_C_dot_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b, int n);
void QLA_DF_c_xeq_C_dot_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b, int *index, int n);
void QLA_DF_c_veq_pC_dot_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT b, int n);
void QLA_DF_c_veq_C_dot_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT *b, int n);
void QLA_DF_c_veq_pC_dot_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT *b, int n);
void QLA_DF_c_xeq_pC_dot_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT b, int *index, int n);
void QLA_DF_c_xeq_C_dot_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT *b, int *index, int n);
void QLA_DF_c_xeq_pC_dot_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT *b, int *index, int n);

/*  QLA_Complex (c = Re trace adj(a) dot b) */


void QLA_DF_r_eq_re_C_dot_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b);
void QLA_DF_r_veq_re_C_dot_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b, int n);
void QLA_DF_r_xeq_re_C_dot_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT b, int *index, int n);
void QLA_DF_r_veq_re_pC_dot_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT b, int n);
void QLA_DF_r_veq_re_C_dot_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT *b, int n);
void QLA_DF_r_veq_re_pC_dot_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT *b, int n);
void QLA_DF_r_xeq_re_pC_dot_C ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT b, int *index, int n);
void QLA_DF_r_xeq_re_C_dot_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, QLA_F_Complex *QLA_RESTRICT *b, int *index, int n);
void QLA_DF_r_xeq_re_pC_dot_pC ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, QLA_F_Complex *QLA_RESTRICT *b, int *index, int n);

/*---------------------------------------------------*/
/* Global sum float to double */
/*---------------------------------------------------*/


/*  QLA_Real (r = sum a) */


void QLA_DF_r_eq_sum_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a);
void QLA_DF_r_veq_sum_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int n);
void QLA_DF_r_xeq_sum_R ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT a, int *index, int n);
void QLA_DF_r_veq_sum_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int n);
void QLA_DF_r_xeq_sum_pR ( QLA_D_Real *QLA_RESTRICT r, QLA_F_Real *QLA_RESTRICT *a, int *index, int n);

/*  QLA_Complex (r = sum a) */


void QLA_DF_c_eq_sum_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a);
void QLA_DF_c_veq_sum_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int n);
void QLA_DF_c_xeq_sum_C ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT a, int *index, int n);
void QLA_DF_c_veq_sum_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int n);
void QLA_DF_c_xeq_sum_pC ( QLA_D_Complex *QLA_RESTRICT r, QLA_F_Complex *QLA_RESTRICT *a, int *index, int n);
#endif /* _QLA_DF_H */
