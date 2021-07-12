/******* dslash_fn2.c - dslash for improved KS fermions T3E version ****/
/* MIMD version 6 */
/* Kogut-Susskind fermions -- improved.  This version for "fat plus
   Naik" quark action.  Connection to nearest neighbors stored in
   fatlink and to third nearest neighbors in longlink */

/* With DSLASH_TMP_LINKS, assumes that the gauge links have been
   prestored in t_fatlinks and t_longlinks.  Otherwise, takes the
   fatlinks and longlinks from the site structure. */

/* This version waits for gathers from both positive and negative
   directions before computing, thereby combining two lattice loops in
   an attempt to gain prefetching time for sub_four_su3_vecs */

/* Jim Hetrick, Kari Rummukainen, Doug Toussaint, Steven Gottlieb */
/* C. DeTar 9/29/01 Standardized prefetching and synced the versions */

#include "generic_ks_includes.h"	/* definitions files and prototypes */
#define LOOPEND
#include "loopend.h"
#define FETCH_UP 1

#define INDEX_3RD(dir) (dir - 8)      /* this gives the 'normal' direction */

/* Temporary work space for dslash_fn_on_temp_special */ 
static su3_vector *temp[9] ;
/* Flag indicating if temp is allocated               */
static int temp_not_allocated=1 ;

void cleanup_gathers(msg_tag *tags1[], msg_tag *tags2[])
{
  int i;

  for(i=XUP;i<=TUP;i++){
    cleanup_gather( tags1[i] );
    cleanup_gather( tags1[OPP_DIR(i)] );
    cleanup_gather( tags2[i] );
    cleanup_gather( tags2[OPP_DIR(i)] );
  }

  for(i=X3UP;i<=T3UP;i++){
    cleanup_gather( tags1[i] );
    cleanup_gather( tags1[OPP_3_DIR(i)] );
    cleanup_gather( tags2[i] );
    cleanup_gather( tags2[OPP_3_DIR(i)] );
  }
}

void cleanup_dslash_temps(){
  register int i ;
  if(!temp_not_allocated)
    for(i=0;i<9;i++) {
      free(temp[i]) ; 
    }
  temp_not_allocated=1 ;
}


/* D_slash routine - sets dest. on each site equal to sum of
   sources parallel transported to site, with minus sign for transport
   from negative directions.  Use "fatlinks" for one link transport,
   "longlinks" for three link transport. */
void dslash_fn( field_offset src, field_offset dest, int parity ) {
  register int i;
  register site *s;
  register int dir,otherparity;
  register su3_matrix *fat4, *long4;
  msg_tag *tag[16];

  if(!valid_longlinks)load_longlinks();
  if(!valid_fatlinks)load_fatlinks();
  switch(parity){
    case EVEN:	otherparity=ODD; break;
    case ODD:	otherparity=EVEN; break;
    case EVENANDODD:	otherparity=EVENANDODD; break;
  }

  /* Start gathers from positive directions */
  /* And start the 3-step gather too */
  for( dir=XUP; dir<=TUP; dir++ ){
    tag[dir] = start_gather( src, sizeof(su3_vector), dir, parity,
        gen_pt[dir] );
    tag[DIR3(dir)] = start_gather( src, sizeof(su3_vector), DIR3(dir),
        parity, gen_pt[DIR3(dir)] );
  }

  /* Multiply by adjoint matrix at other sites */
  /* Use fat link for single link transport */
  FORSOMEPARITY( i, s, otherparity ){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_adj_su3_mat_vec_4dir( fat4,
        (su3_vector *)F_PT(s,src), s->tempvec );
    /* multiply by 3-link matrices too */
    mult_adj_su3_mat_vec_4dir( long4,
        (su3_vector *)F_PT(s,src), s->templongvec );
  } END_LOOP

  /* Start gathers from negative directions */
  for( dir=XUP; dir <= TUP; dir++){
    tag[OPP_DIR(dir)] = start_gather( F_OFFSET(tempvec[dir]),
        sizeof(su3_vector), OPP_DIR( dir), parity,
        gen_pt[OPP_DIR(dir)] );
  }

  /* Start 3-neighbour gathers from negative directions */
  for( dir=X3UP; dir <= T3UP; dir++){
    tag[OPP_3_DIR(dir)] 
      = start_gather( F_OFFSET(templongvec[INDEX_3RD(dir)]),
          sizeof(su3_vector), OPP_3_DIR( dir), parity,
          gen_pt[OPP_3_DIR(dir)] );
  }

  /* Wait gathers from positive directions, multiply by matrix and
     accumulate */
  /* wait for the 3-neighbours from positive directions, multiply */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[dir]);
    wait_gather(tag[DIR3(dir)]);
  }
  /* Wait gathers from negative directions, accumulate (negative) */
  /* and the same for the negative 3-rd neighbours */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[OPP_DIR(dir)]);
  }
  for(dir=X3UP; dir<=T3UP; dir++){
    wait_gather(tag[OPP_3_DIR(dir)]);
  }


  FORSOMEPARITY(i,s,parity){
#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_su3_mat_vec_sum_4dir( fat4,
        (su3_vector *)gen_pt[XUP][i], (su3_vector *)gen_pt[YUP][i],
        (su3_vector *)gen_pt[ZUP][i], (su3_vector *)gen_pt[TUP][i],
        (su3_vector *)F_PT(s,dest));

    mult_su3_mat_vec_sum_4dir( long4,
        (su3_vector *)gen_pt[X3UP][i], (su3_vector *)gen_pt[Y3UP][i],
        (su3_vector *)gen_pt[Z3UP][i], (su3_vector *)gen_pt[T3UP][i],
        (su3_vector *) &(s->templongv1));

    sub_four_su3_vecs( (su3_vector *)F_PT(s,dest),
        (su3_vector *)(gen_pt[XDOWN][i]),
        (su3_vector *)(gen_pt[YDOWN][i]),
        (su3_vector *)(gen_pt[ZDOWN][i]),
        (su3_vector *)(gen_pt[TDOWN][i]) );
    sub_four_su3_vecs( &(s->templongv1), 
        (su3_vector *)(gen_pt[X3DOWN][i]),
        (su3_vector *)(gen_pt[Y3DOWN][i]),
        (su3_vector *)(gen_pt[Z3DOWN][i]),
        (su3_vector *)(gen_pt[T3DOWN][i]) );
    /* Now need to add these things together */
    add_su3_vector((su3_vector *)F_PT(s,dest), & (s->templongv1),
        (su3_vector *)F_PT(s,dest));
  } END_LOOP

  /* free up the buffers */
  for(dir=XUP; dir<=TUP; dir++){
    cleanup_gather(tag[dir]);
    cleanup_gather(tag[OPP_DIR(dir)]);
  }
  for(dir=X3UP; dir<=T3UP; dir++){
    cleanup_gather(tag[dir]);
    cleanup_gather(tag[OPP_3_DIR(dir)]);
  }
}

/* Special dslash for use by congrad.  Uses restart_gather() when
   possible. Last argument is an array of message tags, to be set
   if this is the first use, otherwise reused. If start=1,use
   start_gather, otherwise use restart_gather. 
   The calling program must clean up the gathers! */
void dslash_fn_special( field_offset src, field_offset dest,
    int parity, msg_tag **tag, int start ){
  register int i;
  register site *s;
  register int dir,otherparity;
  register su3_matrix *fat4, *long4;

  if(!valid_longlinks)load_longlinks();
  if(!valid_fatlinks)load_fatlinks();
  switch(parity){
    case EVEN:	otherparity=ODD; break;
    case ODD:	otherparity=EVEN; break;
    case EVENANDODD:	otherparity=EVENANDODD; break;
  }

  /* Start gathers from positive directions */
  for(dir=XUP; dir<=TUP; dir++){
    /**printf("dslash_special: up gathers, start=%d\n",start);**/
    if(start==1) tag[dir] = start_gather( src, sizeof(su3_vector),
        dir, parity, gen_pt[dir] );
    else restart_gather( src, sizeof(su3_vector),
        dir, parity, gen_pt[dir] , tag[dir] ); 
  }

  /* and start the 3rd neighbor gather */
  for(dir=X3UP; dir<=T3UP; dir++){
    if(start==1) tag[dir] = start_gather( src, sizeof(su3_vector),
        dir, parity, gen_pt[dir] );
    else restart_gather( src, sizeof(su3_vector),
        dir, parity, gen_pt[dir] , tag[dir] ); 
  }

  /* Multiply by adjoint matrix at other sites */
  FORSOMEPARITY(i,s,otherparity){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_adj_su3_mat_vec_4dir( fat4,
        (su3_vector *)F_PT(s,src), s->tempvec );
    /* multiply by 3-link matrices too */
    mult_adj_su3_mat_vec_4dir( long4,
        (su3_vector *)F_PT(s,src), s->templongvec );
  } END_LOOP

  /* Start gathers from negative directions */
  for( dir=XUP; dir <= TUP; dir++){
    /**printf("dslash_special: down gathers, start=%d\n",start);**/
    if (start==1) tag[OPP_DIR(dir)] = start_gather( F_OFFSET(tempvec[dir]),
        sizeof(su3_vector), OPP_DIR( dir), parity, gen_pt[OPP_DIR(dir)] );
    else restart_gather( F_OFFSET(tempvec[dir]), sizeof(su3_vector),
        OPP_DIR( dir), parity, gen_pt[OPP_DIR(dir)] , tag[OPP_DIR(dir)] );
  }

  /* and 3rd neighbours */
  for( dir=X3UP; dir <= T3UP; dir++){
    /**printf("dslash_special: down gathers, start=%d\n",start);**/
    if (start==1) tag[OPP_3_DIR(dir)] = 
      start_gather( F_OFFSET(templongvec[INDEX_3RD(dir)]),
          sizeof(su3_vector), OPP_3_DIR(dir), parity, gen_pt[OPP_3_DIR(dir)] );
    else restart_gather( F_OFFSET(templongvec[INDEX_3RD(dir)]),
        sizeof(su3_vector), OPP_3_DIR( dir), parity, gen_pt[OPP_3_DIR(dir)],
        tag[OPP_3_DIR(dir)] );
  }

  /* Wait gathers from positive directions, multiply by matrix and
     accumulate */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[dir]);
  }

  /* wait for the 3-neighbours from positive directions, multiply */
  for(dir=X3UP; dir<=T3UP; dir++){
    wait_gather(tag[dir]);
  }

  /* Wait gathers from negative directions, accumulate (negative) */
  /* and the same for the negative 3-rd neighbours */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[OPP_DIR(dir)]);
  } 
  for(dir=X3UP; dir<=T3UP; dir++){
    wait_gather(tag[OPP_3_DIR(dir)]);
  }

  FORSOMEPARITY(i,s,parity){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_su3_mat_vec_sum_4dir( fat4,
        (su3_vector *)gen_pt[XUP][i], (su3_vector *)gen_pt[YUP][i],
        (su3_vector *)gen_pt[ZUP][i], (su3_vector *)gen_pt[TUP][i],
        (su3_vector *)F_PT(s,dest));
    mult_su3_mat_vec_sum_4dir( long4,
        (su3_vector *)gen_pt[X3UP][i], (su3_vector *)gen_pt[Y3UP][i],
        (su3_vector *)gen_pt[Z3UP][i], (su3_vector *)gen_pt[T3UP][i],
        (su3_vector *) &(s->templongv1));

    sub_four_su3_vecs( (su3_vector *)F_PT(s,dest),
        (su3_vector *)(gen_pt[XDOWN][i]),
        (su3_vector *)(gen_pt[YDOWN][i]),
        (su3_vector *)(gen_pt[ZDOWN][i]),
        (su3_vector *)(gen_pt[TDOWN][i]) );
    sub_four_su3_vecs( & (s->templongv1), 
        (su3_vector *)(gen_pt[X3DOWN][i]),
        (su3_vector *)(gen_pt[Y3DOWN][i]),
        (su3_vector *)(gen_pt[Z3DOWN][i]),
        (su3_vector *)(gen_pt[T3DOWN][i]) );
    /*** Now need to add these things together ***/
    add_su3_vector((su3_vector *)F_PT(s,dest), &(s->templongv1),
        (su3_vector *)F_PT(s,dest));
  } END_LOOP

}

void dslash_fn_on_temp( su3_vector *src, su3_vector *dest, int parity ) {
  register int i;
  register site *s;
  register int dir,otherparity;
  msg_tag *tag[16];
  su3_vector *tempvec[4], *templongvec[4], *templongv1 ;
  register su3_matrix *fat4, *long4;

  for( dir=XUP; dir<=TUP; dir++ )
  {
    tempvec[dir]    =(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    templongvec[dir]=(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
  }
  templongv1=(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));

  if(!valid_longlinks)load_longlinks();
  if(!valid_fatlinks)load_fatlinks();
  switch(parity)
  {
    case EVEN:	otherparity=ODD; break;
    case ODD:	otherparity=EVEN; break;
    case EVENANDODD:	otherparity=EVENANDODD; break;
  }

  /* Start gathers from positive directions */
  /* And start the 3-step gather too */
  for( dir=XUP; dir<=TUP; dir++ ){
    tag[dir] = start_gather_from_temp( src, sizeof(su3_vector), dir, parity,
        gen_pt[dir] );
    tag[DIR3(dir)] = start_gather_from_temp( src, sizeof(su3_vector), 
        DIR3(dir),parity, 
        gen_pt[DIR3(dir)] );
  }

  /* Multiply by adjoint matrix at other sites */
  /* Use fat link for single link transport */
  FORSOMEPARITY( i, s, otherparity ){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_adj_su3_mat_4vec( fat4, &(src[i]), &(tempvec[0][i]),
        &(tempvec[1][i]), &(tempvec[2][i]), 
        &(tempvec[3][i]) );
    /* multiply by 3-link matrices too */
    mult_adj_su3_mat_4vec( long4, &(src[i]),&(templongvec[0][i]),
        &(templongvec[1][i]), &(templongvec[2][i]), 
        &(templongvec[3][i]) );
  } END_LOOP

  /* Start gathers from negative directions */
  for( dir=XUP; dir <= TUP; dir++){
    tag[OPP_DIR(dir)] = start_gather_from_temp( tempvec[dir],
        sizeof(su3_vector), OPP_DIR( dir), parity, gen_pt[OPP_DIR(dir)] );
  }

  /* Start 3-neighbour gathers from negative directions */
  for( dir=X3UP; dir <= T3UP; dir++){
    tag[OPP_3_DIR(dir)]=start_gather_from_temp(templongvec[INDEX_3RD(dir)],
        sizeof(su3_vector), OPP_3_DIR( dir), parity, gen_pt[OPP_3_DIR(dir)] );
  }

  /* Wait gathers from positive directions, multiply by matrix and
     accumulate */
  /* wait for the 3-neighbours from positive directions, multiply */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[dir]);
    wait_gather(tag[DIR3(dir)]);
  }

  FORSOMEPARITY(i,s,parity){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_su3_mat_vec_sum_4dir( fat4,
        (su3_vector *)gen_pt[XUP][i], (su3_vector *)gen_pt[YUP][i],
        (su3_vector *)gen_pt[ZUP][i], (su3_vector *)gen_pt[TUP][i],
        &(dest[i]) );

    mult_su3_mat_vec_sum_4dir( long4,
        (su3_vector *)gen_pt[X3UP][i], (su3_vector *)gen_pt[Y3UP][i],
        (su3_vector *)gen_pt[Z3UP][i], (su3_vector *)gen_pt[T3UP][i],
        &(templongv1[i]));
  } END_LOOP

  /* Wait gathers from negative directions, accumulate (negative) */
  /* and the same for the negative 3-rd neighbours */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[OPP_DIR(dir)]);
  }
  for(dir=X3UP; dir<=T3UP; dir++){
    wait_gather(tag[OPP_3_DIR(dir)]);
  }

  FORSOMEPARITY(i,s,parity){

    sub_four_su3_vecs( &(dest[i]),
        (su3_vector *)(gen_pt[XDOWN][i]),
        (su3_vector *)(gen_pt[YDOWN][i]),
        (su3_vector *)(gen_pt[ZDOWN][i]),
        (su3_vector *)(gen_pt[TDOWN][i]) );
    sub_four_su3_vecs( &(templongv1[i]), 
        (su3_vector *)(gen_pt[X3DOWN][i]),
        (su3_vector *)(gen_pt[Y3DOWN][i]),
        (su3_vector *)(gen_pt[Z3DOWN][i]),
        (su3_vector *)(gen_pt[T3DOWN][i]) );
    /* Now need to add these things together */
    add_su3_vector(&(dest[i]), &(templongv1[i]),&(dest[i]));
  } END_LOOP 

  /* free up the buffers */
  for(dir=XUP; dir<=TUP; dir++){
    cleanup_gather(tag[dir]);
    cleanup_gather(tag[OPP_DIR(dir)]);
  }

  for(dir=X3UP; dir<=T3UP; dir++){
    cleanup_gather(tag[dir]);
    cleanup_gather(tag[OPP_3_DIR(dir)]);
  }

  for( dir=XUP; dir<=TUP; dir++ ){
    free(tempvec[dir]);
    free(templongvec[dir]);
  }
  free(templongv1);
}

/* Special dslash for use by congrad.  Uses restart_gather() when
   possible. Next to last argument is an array of message tags, to be set
   if this is the first use, otherwise reused. If start=1,use
   start_gather, otherwise use restart_gather. 
   The calling program must clean up the gathers and temps! */
void dslash_fn_on_temp_special(su3_vector *src, su3_vector *dest,
    int parity, msg_tag **tag, int start ){
  register int i;
  register site *s;
  register int dir,otherparity;
  register su3_matrix *fat4, *long4;

  /* allocate temporary work space only if not already allocated */
  if(temp_not_allocated)
  {
    for( dir=XUP; dir<=TUP; dir++ ){
      temp[dir]  =(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
      temp[dir+4]=(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    }
    temp[8]=(su3_vector *)calloc(sites_on_node, sizeof(su3_vector));
    temp_not_allocated = 0 ;
  }

  /* load fatlinks and longlinks */
  if(!valid_longlinks) load_longlinks();
  if(!valid_fatlinks) load_fatlinks();

  switch(parity)
  {
    case EVEN:	otherparity=ODD; break;
    case ODD:	otherparity=EVEN; break;
    case EVENANDODD:	otherparity=EVENANDODD; break;
  }

  /* Start gathers from positive directions */
  /* And start the 3-step gather too */
  for( dir=XUP; dir<=TUP; dir++ ){
    if(start==1)
    {
      tag[dir] = start_gather_from_temp( src, sizeof(su3_vector), 
          dir, parity,gen_pt[dir] );
      tag[DIR3(dir)] = start_gather_from_temp(src, sizeof(su3_vector),
          DIR3(dir),parity, 
          gen_pt[DIR3(dir)] );
    }
    else
    {
      restart_gather_from_temp( src, sizeof(su3_vector), 
          dir, parity,gen_pt[dir], tag[dir]);
      restart_gather_from_temp(src, sizeof(su3_vector), DIR3(dir), parity, 
          gen_pt[DIR3(dir)], tag[DIR3(dir)]);
    }
  }

  /* Multiply by adjoint matrix at other sites */
  /* Use fat link for single link transport */
  FORSOMEPARITY( i, s, otherparity ){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_adj_su3_mat_4vec( fat4, &(src[i]), &(temp[0][i]),
        &(temp[1][i]), &(temp[2][i]), &(temp[3][i]) );
    /* multiply by 3-link matrices too */
    mult_adj_su3_mat_4vec( long4, &(src[i]),&(temp[4][i]),
        &(temp[5][i]), &(temp[6][i]), &(temp[7][i]) );
  } END_LOOP

  /* Start gathers from negative directions */
  for( dir=XUP; dir <= TUP; dir++){
    if (start==1) tag[OPP_DIR(dir)] = start_gather_from_temp( temp[dir],
        sizeof(su3_vector), OPP_DIR( dir), parity, gen_pt[OPP_DIR(dir)] );
    else restart_gather_from_temp( temp[dir], sizeof(su3_vector), 
        OPP_DIR( dir), parity, gen_pt[OPP_DIR(dir)], tag[OPP_DIR(dir)] );
  }

  /* Start 3-neighbour gathers from negative directions */
  for( dir=X3UP; dir <= T3UP; dir++){
    if (start==1) tag[OPP_3_DIR(dir)]=start_gather_from_temp(
        temp[INDEX_3RD(dir)+4], sizeof(su3_vector), 
        OPP_3_DIR( dir), parity, gen_pt[OPP_3_DIR(dir)] );
    else restart_gather_from_temp(temp[INDEX_3RD(dir)+4], 
        sizeof(su3_vector), OPP_3_DIR( dir),parity, 
        gen_pt[OPP_3_DIR(dir)], tag[OPP_3_DIR(dir)] );
  }

  /* Wait gathers from positive directions, multiply by matrix and
     accumulate */
  /* wait for the 3-neighbours from positive directions, multiply */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[dir]);
    wait_gather(tag[DIR3(dir)]);
  }

  FORSOMEPARITY(i,s,parity){

#ifdef DSLASH_TMP_LINKS
    fat4 = &(t_fatlink[4*i]);
    long4 = &(t_longlink[4*i]);
#else
    fat4 = s->fatlink;
    long4 = s->longlink;
#endif
    mult_su3_mat_vec_sum_4dir( fat4,
        (su3_vector *)gen_pt[XUP][i], (su3_vector *)gen_pt[YUP][i],
        (su3_vector *)gen_pt[ZUP][i], (su3_vector *)gen_pt[TUP][i],
        &(dest[i]) );

    mult_su3_mat_vec_sum_4dir( long4,
        (su3_vector *)gen_pt[X3UP][i], (su3_vector *)gen_pt[Y3UP][i],
        (su3_vector *)gen_pt[Z3UP][i], (su3_vector *)gen_pt[T3UP][i],
        &(temp[8][i]));
  } END_LOOP

  /* Wait gathers from negative directions, accumulate (negative) */
  /* and the same for the negative 3-rd neighbours */
  for(dir=XUP; dir<=TUP; dir++){
    wait_gather(tag[OPP_DIR(dir)]);
  }
  for(dir=X3UP; dir<=T3UP; dir++){
    wait_gather(tag[OPP_3_DIR(dir)]);
  }

  FORSOMEPARITY(i,s,parity){

    sub_four_su3_vecs( &(dest[i]),
        (su3_vector *)(gen_pt[XDOWN][i]),
        (su3_vector *)(gen_pt[YDOWN][i]),
        (su3_vector *)(gen_pt[ZDOWN][i]),
        (su3_vector *)(gen_pt[TDOWN][i]) );
    sub_four_su3_vecs( &(temp[8][i]), 
        (su3_vector *)(gen_pt[X3DOWN][i]),
        (su3_vector *)(gen_pt[Y3DOWN][i]),
        (su3_vector *)(gen_pt[Z3DOWN][i]),
        (su3_vector *)(gen_pt[T3DOWN][i]) );
    /* Now need to add these things together */
    add_su3_vector(&(dest[i]), &(temp[8][i]),&(dest[i]));
  } END_LOOP 

}
