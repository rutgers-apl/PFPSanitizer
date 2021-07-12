#ifndef _CONFIG_H
#define _CONFIG_H
/* config.h.  For now, NOT generated automatically by configure.  */

/* Collects macros for preprocessor tweaks that accommodate
   differences in compilers, architecture and OS */

/********************************************************************/
/* Compiler/Processor-dependent macros */
/********************************************************************/

/* Specify the unsigned 32 bit integer base type for this compiler */
/* Run the script "getint.sh" to find out what to use */
/* One and only one of these should be defined */
#define INT_IS_32BIT 1
#undef SHORT_IS_32BIT   /* Needed on T3E UNICOS, for example */

/* Define if the target processor has native double precision */
/* (For some library routines, gives slightly better performance) */
/* #undef NATIVEDOUBLE */

/* Define if the cache line is 64 bytes (if not, we assume 32 bytes). */
/* Processors that do: P4 (actually fetches 128), EV67, EV68  */
/* Used only for prefetching, so it only affects performance */
/* #undef HAVE_64_BYTE_CACHELINE */

/********************************************************************/
/* Compiler/OS-dependent macros */
/********************************************************************/

/* Define if you have the <ieeefp.h> header file. */
/* Systems that don't: T3E UNICOS, Exemplar, Linux gcc, SP AIX */
#define HAVE_IEEEFP_H 1

#ifndef SPEC_CPU
/* Define if you have the <unistd.h> header file. */
/* Systems that don't: NT */
/*#define HAVE_UNISTD_H 1*/

/* Added for SPEC CPU2006*/
#define HAVE_UNISTD_H 1

/* Define if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1
#endif /* !SPEC_CPU */

/* Define if you have ANSI "fseeko" */
/* #undef HAVE_FSEEKO */  
/* Systems that don't: T3E UNICOS */
#define HAVE_FSEEKO 1

#endif /* _CONFIG_H */

