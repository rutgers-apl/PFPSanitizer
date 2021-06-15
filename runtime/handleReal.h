#ifndef __FPSAN_RUNTIME_H__
#define __FPSAN_RUNTIME_H__
#include "tbb/concurrent_queue.h"
#include <asm/unistd.h>
#include <assert.h>
#include <execinfo.h>
#include <fstream>
#include <gmp.h>
#include <iostream>
#include <limits.h>
#include <linux/perf_event.h>
#include <list>
#include <map>
#include <math.h>
#include <mpfr.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MMAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE)
#define MAX_STACK_SIZE 20
using namespace tbb;

#ifdef DEFAULT_NUM_THREADS
#define NUM_THREADS 1 
#endif

#if !defined(NUM_THREADS)
#ifdef NUM_T_1
#define NUM_THREADS 0
#endif
#ifdef NUM_T_4
#define NUM_THREADS 3
#endif
#ifdef NUM_T_8
#define NUM_THREADS 7
#endif
#ifdef NUM_T_16
#define NUM_THREADS 15
#endif
#ifdef NUM_T_32
#define NUM_THREADS 31
#endif
#ifdef NUM_T_64
#define NUM_THREADS 63
#endif
#endif

#define NUM_QUEUES NUM_THREADS * 2 + 1

#ifdef DEFAULT_BUF_SIZE
#define BUF_SIZE 8000000
#endif

#ifdef EXPANDED_BUF_SIZE
#define BUF_SIZE 800000000 
#endif


/* 32 million entries in the hash table */
const size_t HASH_TABLE_ENTRIES = ((size_t)1024 * (size_t)1024);
/* using just 1024 entries gives 11X slowdown on average and 13X with 1024*1024 entries 
 * Reducing entries to 126, won't reduce slowdown from 11X. That means there is some other 
 * source of slowdown.
 * */

#define debug 0
#define debugtrace 0
#define debugerror 0

#define ERRORTHRESHOLD 50
size_t fpcount = 0;
size_t varCount = 0;
typedef void (*func_ptr_t)(int);
pthread_t thread[NUM_THREADS];
concurrent_bounded_queue<std::pair<func_ptr_t, int>> task_queue;
concurrent_bounded_queue<int> empty_list;
std::map<size_t, void *> clone_func_map[NUM_QUEUES];
std::map<size_t, int> error_map[NUM_QUEUES];
std::map<size_t, int> cc_map[NUM_QUEUES];
__thread size_t __buf_idx;

bool m_start_slice = false;
size_t countFInst = 0;
FILE *m_errfile_c;
FILE *m_errfile_p;
FILE *m_errfile[NUM_QUEUES];
FILE *m_fpcore[NUM_QUEUES];
bool m_init_flag[NUM_QUEUES];
int p_queue_idx = -1;
enum fp_op {
  FADD,
  FSUB,
  FMUL,
  FDIV,
  CONSTANT,
  SQRT,
  CBRT,
  FMA,
  CEIL,
  FLOOR,
  TAN,
  SIN,
  COS,
  ATAN,
  ABS,
  LOG,
  LOG10,
  ASIN,
  EXP,
  POW,
  MIN,
  MAX,
  LDEXP,
  FMOD,
  ATAN2,
  HYPOT,
  COSH,
  SINH,
  TANH,
  ACOS,
  UNKNOWN
};

/* smem_entry: metadata maintained with each shadow memory location.
 * val   : mpfr value for the shadow execution
 * computed: double value
 * lineno: line number in the source file
 * is_init: is the MPFR initialized
 * opcode: opcode of the operation that created the result

 * error : number of bits in error. Why is it here? (TRACING)
 * lock: CETS style metadata for validity of the temporary metadata pointer
 (TRACING)
 * key:  CETS style metadata for validity of the temporary metadata pointer
 (TRACING)
 * tmp_ptr: Pointer to the metadata of the temporary  (TRACING)
 */
struct smem_entry {

  mpfr_t val;
  double computed;
  size_t inst_id;
  enum fp_op opcode;
  bool is_init;

#ifdef TRACING
  int error;
  unsigned int lock;
  unsigned int key;
  struct temp_entry *tmp_ptr;
#endif
};

struct temp_entry {
  mpfr_t val;
  double computed;
  size_t inst_id;
  enum fp_op opcode;
#ifdef TRACING
  int error;
  size_t lock;
  size_t key;

  size_t op1_lock;
  size_t op1_key;
  struct temp_entry *lhs;

  size_t op2_lock;
  size_t op2_key;
  struct temp_entry *rhs;
  size_t timestamp;
#endif
};

struct fat_mpfr_t {
  mpfr_t val;
  size_t padding[15];
};

struct fat_size_t {
  size_t index;
  size_t padding[15];
};


#if !defined(PREC_128) && !defined(PREC_256) && !defined(PREC_512) &&          \
    !defined(PREC_1024)
#define PREC_512
#endif

#if !defined(PRECISION)
#ifdef PREC_128
#define PRECISION 128
#endif
#ifdef PREC_256
#define PRECISION 256
#endif
#ifdef PREC_512
#define PRECISION 512
#endif
#ifdef PREC_1024
#define PRECISION 1024
#endif
#endif

#ifdef TRACING
size_t *m_lock_key_map[NUM_QUEUES];
#endif

/*
We don't want to call mpfr_init on every add or sub.That's why we keep
it as global variables and do init once and just update on every add or sub
*/
fat_mpfr_t op1_mpfr[NUM_QUEUES], op2_mpfr[NUM_QUEUES], res_mpfr[NUM_QUEUES];
fat_mpfr_t computed[NUM_QUEUES], temp_diff[NUM_QUEUES];

struct timeval tv1[NUM_QUEUES];
struct timeval tv2[NUM_QUEUES];

temp_entry *m_shadow_stack[NUM_QUEUES];
smem_entry *m_shadow_memory[NUM_QUEUES];
size_t *m_func_addr_map[NUM_QUEUES];

std::string varString;
std::map<temp_entry*, std::string> m_var_map;
func_ptr_t func_addr_queue[NUM_QUEUES];

int fini = 0;
//No contention for queue, replaced with fat_double_t queue[NUM_QUEUES] 
//and there was no change in performance.
double *queue[NUM_QUEUES];
fat_size_t q_w_idx[NUM_QUEUES];
fat_size_t q_r_idx[NUM_QUEUES];

fat_size_t m_prec_bits_f[NUM_QUEUES];
fat_size_t m_prec_bits_d[NUM_QUEUES];
fat_size_t m_precision[NUM_QUEUES];

#ifdef TRACING
fat_size_t m_timestamp[NUM_QUEUES];
fat_size_t m_key_stack_top[NUM_QUEUES];
fat_size_t m_key_counter[NUM_QUEUES];
#endif

fat_size_t infCount[NUM_QUEUES];
fat_size_t nanCount[NUM_QUEUES];
fat_size_t errorCount[NUM_QUEUES];
fat_size_t flipsCount[NUM_QUEUES];
fat_size_t ccCount[NUM_QUEUES];

std::list<temp_entry *> m_expr[NUM_QUEUES];
std::string m_get_string_opcode(size_t);
unsigned long m_ulpd(double x, double y);
unsigned long m_ulpf(float x, float y);
int m_update_error(int buf_id, temp_entry *mpfr_val, double computedVal);
void m_print_error(size_t opcode, temp_entry *real, double d_value,
                   unsigned int cbad, unsigned long long int instId,
                   bool debugInfoAvail, unsigned int linenumber,
                   unsigned int colnumber);
void m_print_real(mpfr_t);
void m_print_trace(void);
int m_isnan(mpfr_t real);
int m_get_depth(int, temp_entry *);
void m_compute(fp_op, double, temp_entry *, double, temp_entry *, double,
               temp_entry *, size_t);

void m_store_shadow_dconst(int, smem_entry *, double, unsigned int);
void m_store_shadow_fconst(int, smem_entry *, float, unsigned int);
void m_init_store_shadow_fconst(int, smem_entry*, float, size_t, unsigned int);
extern "C" void *m_pull_addr(int buf_id);
size_t m_pull_size(int);
#endif
