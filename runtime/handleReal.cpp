#include "handleReal.h"
#include <linux/perf_event.h>
#include <stdarg.h>
#include <string>
#include <sys/time.h>
#define DEBUG_BUF_P 0
#define DEBUG_BUF_C 0
#define DEBUG_PROTO_P 0
#define DEBUG_PROTO_C 0
#define PROFILE 0

#ifdef TRACING

/* This function convert the FP opcode to string to print while tracing the error.
 * */
std::string m_get_string_opcode_fpcore(size_t opcode){
  switch(opcode){
    case FADD:
      return "+";
    case FMUL:
      return "*";
    case FSUB:
      return "-";
    case FDIV:
      return "/";
    case CONSTANT:
      return "CONSTANT";
    case SQRT:
      return "sqrt";
    case CBRT:
      return "cbrt";
    case FMA:
      return "fma";
    case FLOOR:
      return "floor";
    case TAN:
      return "tan";
    case SIN:
      return "sin";
    case COS:
      return "cos";
    case ATAN:
      return "atan";
    case ABS:
      return "abs";
    case LOG:
      return "log";
    case ASIN:
      return "asin";
    case EXP:
      return "exp";
    case POW:
      return "pow";
    case FMOD:
      return "fmod";
    default:
      return "Unknown";
  }
}

/* This function prints the expression in fpcore representation,
 * so that Herbie can be used to rewrite the expression 
 * */
extern "C" void fpsan_fpcore(int buf_id, temp_entry *cur){
  if(cur){
    if(m_lock_key_map[buf_id][cur->op1_lock] != cur->op1_key ){
      if(m_var_map.count(cur) > 0){
        varString += "( x_"+ m_var_map.at(cur) + ")";
      }
      else{
        varCount++;
        auto var = std::to_string(varCount);
        varString += "( x_"+ var + ")";
        m_var_map.insert( std::pair<temp_entry*, std::string>(cur, var) );
      }
      return;
    }
    if(m_lock_key_map[buf_id][cur->op2_lock] != cur->op2_key){
      if(m_var_map.count(cur) > 0){
        varString += "( x_"+ m_var_map.at(cur) + ")";
      }
      else{
        varCount++;
        auto var = std::to_string(varCount);
        varString += "( x_"+ var + ")";
        m_var_map.insert( std::pair<temp_entry*, std::string>(cur, var) );
      }
      return;
    }
    if(cur->opcode == CONSTANT){
      if(m_var_map.count(cur) > 0){
        varString += "( x_"+ m_var_map.at(cur);
      }
      else{
        varCount++;
        auto var = std::to_string(varCount);
        varString += "( x_"+ var ;
        m_var_map.insert( std::pair<temp_entry*, std::string>(cur, var) );
      }
    }
    else{
      varString += "(" + m_get_string_opcode_fpcore(cur->opcode);
    }
    if(cur->lhs != NULL){
      if(cur->lhs->timestamp < cur->timestamp){
        fpsan_fpcore(buf_id, cur->lhs);
      }
    }
    if(cur->rhs != NULL){
      if(cur->rhs->timestamp < cur->timestamp){
        fpsan_fpcore(buf_id, cur->rhs);
      }
    }
    varString += ")";
  }
}

extern "C" void fpsan_get_fpcore(int buf_id, temp_entry *cur){
  fflush(stdout);
  fpsan_fpcore(buf_id, cur);
  std::string out_fpcore;
  out_fpcore = "(FPCore ( ";

  while(varCount > 0){
    out_fpcore += "x_"+std::to_string(varCount) + " ";
    varCount--;
  }
  out_fpcore += ")\n";
  out_fpcore += varString;
  out_fpcore += ")\n";
  fprintf(m_fpcore[buf_id], "%s",out_fpcore.c_str());
  varString = "";
  varCount = 0;
}

/* This function trace back the error and print the DAG with 
 * opcode, instruction id, operands instruction id and error occured. 
 * */
extern "C" void fpsan_trace(int buf_id, temp_entry *current) {
  m_expr[buf_id].push_back(current);
  int level;
  while (!m_expr[buf_id].empty()) {
    level = m_expr[buf_id].size();
    temp_entry *cur = m_expr[buf_id].front();
    std::cout << "\n";
    if (cur == NULL) {
      return;
    }
    std::cout << " " << cur->inst_id << " " << m_get_string_opcode(cur->opcode)
              << " ";
    fflush(stdout);
    if (m_lock_key_map[buf_id][cur->op1_lock] != cur->op1_key) {
      return;
    }
    if (m_lock_key_map[buf_id][cur->op2_lock] != cur->op2_key) {
      return;
    }
    if (cur->lhs != NULL) {
      std::cout << " " << cur->lhs->inst_id << " ";
      if (cur->lhs->timestamp < cur->timestamp) {
        m_expr[buf_id].push_back(cur->lhs);
        fflush(stdout);
      }
    }
    if (cur->rhs != NULL) {
      std::cout << " " << cur->rhs->inst_id << " ";
      if (cur->rhs->timestamp < cur->timestamp) {
        m_expr[buf_id].push_back(cur->rhs);
        fflush(stdout);
      }
    }
    std::cout << "(real:";
    m_print_real(cur->val);
    printf(" computed: %.17g", cur->computed);
    std::cout << ", error:" << m_update_error(buf_id, cur, cur->computed) << " "
              << ")";
    fflush(stdout);
    m_expr[buf_id].pop_front();
    level--;
  }
  int depth = m_get_depth(buf_id, current);
  std::cout << "depth:" << depth << "\n";
}
#endif

// fpsan_check_branch, fpsan_check_conversion, fpsan_check_error are
// functions that user can set breakpoint on
extern "C" unsigned int fpsan_check_branch(bool realBr, bool computedBr,
                                           temp_entry *realRes1,
                                           temp_entry *realRes2) {
  if (realBr != computedBr) {
    return 1;
  }
  return 0;
}

extern "C" unsigned int fpsan_check_conversion(long real, long computed,
                                               temp_entry *realRes) {
  if (real != computed) {
    return 1;
  }
  return 0;
}

extern "C" void fpsan_check_error(int buf_id, temp_entry *realRes){
#ifdef TRACING
  if (debugtrace) {
    std::cout << "m_expr starts\n";
    m_expr[buf_id].clear();
    fpsan_trace(buf_id, realRes);
    std::cout << "\nm_expr ends\n";
    std::cout << "\n";
  }

  m_update_error(buf_id, realRes, realRes->computed);
#endif
}

extern "C" unsigned int fpsan_check_error_f(int buf_id, temp_entry *realRes,
                                            float computedRes) {
#ifdef TRACING
  if (debugtrace) {
    std::cout << "m_expr starts\n";
    m_expr[buf_id].clear();
    fpsan_trace(buf_id, realRes);
    std::cout << "\nm_expr ends\n";
    std::cout << "\n";
  }

  if (realRes->error > ERRORTHRESHOLD)
    return 4;
  return 0;
#else
  int bits_error = m_update_error(buf_id, realRes, computedRes);
  if (bits_error > ERRORTHRESHOLD)
    return 4;

  return 0;
#endif
}

extern "C" unsigned int fpsan_check_error_d(int buf_id, temp_entry *realRes,
                                            double computedRes) {
#ifdef TRACING
  if (debugtrace) {
    std::cout << "m_expr starts\n";
    m_expr[buf_id].clear();
    fpsan_trace(buf_id, realRes);
    std::cout << "\nm_expr ends\n";
    std::cout << "\n";
  }

  if (realRes->error > ERRORTHRESHOLD) {
    return 4;
  }
  return 0;
#else
  int bits_error = m_update_error(buf_id, realRes, computedRes);
  if (bits_error > ERRORTHRESHOLD)
    return 4;
  return 0;
#endif

  return 0;
}

/* This is a consumer thread which run in a infinite loop and threads from the thread pool
 * steals the task from the task queue and execute it 
 * */
static void *worker(void *arg) {
  while (1) {
    if (__atomic_load_n(&(fini), 0) && task_queue.size() == 0) {
      return NULL;
    }
#ifdef LOCK
    
#else
    func_ptr_t func = nullptr;
    int idx = 0;
    auto event = std::pair<func_ptr_t, int>({func, idx});
    // Its important to use try pop rather than pop, in case we have more
    // threads than the task then threads who lost the battle may be stuck here
    // forever if pop is used.
    if (task_queue.try_pop(event)) {

      __buf_idx = event.second;
      //set thread local variable
      if (DEBUG_PROTO_C) {
        std::cout<<"worker witd tid:"<<" starting at:"<<event.second<<"\n";
      }

      event.first(event.second);

      if (DEBUG_PROTO_C) {
        std::cout<<"consumer empty_list: pushing:"<<event.second<<"\n";
      }
      empty_list.push(event.second);
    }
#endif
  }

  return NULL;
}

/* These functions are for debugging purpose.
 * */
extern "C" void fpsanx_print_index(int index) {
    fprintf(m_errfile_p, "fpsanx_print_index:%d\n", index);
}

extern "C" void fpsanx_print_slice_flag(size_t index) {
  if (DEBUG_BUF_P) {
    std::cout<<"index:"<<index<<"\n";
  }
}

extern "C" void pull_print(double val, size_t index) {
  if (DEBUG_BUF_P) {
    fprintf(m_errfile_c, "%f:%lu\n", val, index);
    std::cout<< val<<":"<<index<<"\n";
  }
}
extern "C" void fpsanx_push_print(double val, size_t index) {
  if (DEBUG_BUF_P) {
    fprintf(m_errfile_p, "%f:%lu\n", val, index);
    std::cout<< val<<":"<<index<<"\n";
  }
}

/* This function returns the available empty buffer address to the producer.
 * This function also stores the clone address of the function to handle indirect
 * function calls.
 * */
extern "C" double *fpsanx_get_buf_addr(void *origFuncptr, void *cloneFuncptr) {
  // producer stalls if all queues are full, pop blocks entil item is available
  if (p_queue_idx < 0) {
    empty_list.pop(p_queue_idx);
    if (DEBUG_PROTO_P) {
      std::cout << "producer: starting to push to queue idx:" << p_queue_idx
                << "\n";
    }
  }
  size_t toAddrInt = (size_t)(origFuncptr);

  if(clone_func_map[p_queue_idx].count(toAddrInt) == 0){
    clone_func_map[p_queue_idx].insert(std::pair<size_t, void *>(toAddrInt, cloneFuncptr) );
  }
  return &queue[p_queue_idx][0];
}

/* This function returns the available empty buffer address to the consumer.
 * */
extern "C" double *fpsanx_get_buf_addr_c() {
  if (DEBUG_PROTO_C) {
    std::cout << "consumer: starting to pull from queue idx:" << __buf_idx << "\n";
  }
  return &queue[__buf_idx][0];;
}

/* This function returns the available current buffer index to the consumer.
 * */
extern "C" size_t fpsanx_get_buf_idx_c() {
  if (DEBUG_PROTO_C) {
    std::cout << "consumer: starting to pull from queue idx:" << __buf_idx << "\n";
  }
  return __buf_idx;
}

/* This function is called once slice finishes the execution in the producer.
 * This function pushes the task once it has finished filling the buffer.
 * */
extern "C" void fpsan_slice_end(func_ptr_t slice_shadow) {
  if (DEBUG_PROTO_P) {
    printf("producer: pushing task %p, with queue id:%d\n", slice_shadow,
           p_queue_idx);
  }
#if NUM_THREADS == 0
  __buf_idx = p_queue_idx;
  slice_shadow(p_queue_idx);
  empty_list.push(p_queue_idx);
#else  
  task_queue.push({slice_shadow, p_queue_idx});
#endif
  p_queue_idx = -1;
}

/* If an application calls a library function indirectly, then
 * in shadow slice we won't have the clone function, in that
 * case we return the dummy function.
 * */
extern "C" void dummy(int ){
}

/* This function returns the address of the cloned function for a indirect
 * function call in shadow slice.
 * */
extern "C" void* fpsanx_get_clone_addr(int buf_id, size_t toAddrInt){
  if(clone_func_map[buf_id].count(toAddrInt) == 0){
    func_ptr_t slice_shadow = &dummy; 
    return (void*)slice_shadow;
  }
  return clone_func_map[buf_id][toAddrInt];
}

/* This function is called once shadow slice finishes the execution to
 * record the numerical errors occured during the execution of shadow slice.
 * */
extern "C" void fpsan_shadow_slice_end(int buf_id) {
  if (DEBUG_PROTO_C) {
    std::cout << "consumer: shadow slice finished work on queue:" << buf_id
              << "\n\n\n";
    gettimeofday(&tv2[buf_id], NULL);
    printf("Total time = %f seconds\n",
           (double)(tv2[buf_id].tv_usec - tv1[buf_id].tv_usec) / 1000000 +
               (double)(tv2[buf_id].tv_sec - tv1[buf_id].tv_sec));
  }
  if (PROFILE) {
    std::cout<<"fp:"<<fpcount<<"\n";
    fpcount = 0;
  }
#ifdef DEBUG_ERROR
#else
  #ifdef CORRECTNESS
    fprintf(m_errfile[buf_id], "Error above 45 bits found %zd\n", errorCount[buf_id].index);
    fprintf(m_errfile[buf_id], "Total NaN found %zd\n", nanCount[buf_id].index);
    fprintf(m_errfile[buf_id], "Total Inf found %zd\n", infCount[buf_id].index);
    fprintf(m_errfile[buf_id], "Total branch flips found %zd\n", flipsCount[buf_id].index);
    fprintf(m_errfile[buf_id], "Total catastrophic cancellation found %zd\n",
      ccCount[buf_id].index);

  #else
    if(errorCount[buf_id].index > 0)
      fprintf(m_errfile[buf_id], "Error above 45 bits found %zd\n", errorCount[buf_id].index);
    if(nanCount[buf_id].index > 0)
      fprintf(m_errfile[buf_id], "Total NaN found %zd\n", nanCount[buf_id].index);
    if(infCount[buf_id].index > 0)
      fprintf(m_errfile[buf_id], "Total Inf found %zd\n", infCount[buf_id].index);
    if(flipsCount[buf_id].index > 0)
      fprintf(m_errfile[buf_id], "Total branch flips found %zd\n", flipsCount[buf_id].index);
    if(ccCount[buf_id].index > 0)
      fprintf(m_errfile[buf_id], "Total catastrophic cancellation found %zd\n",
      ccCount[buf_id].index);
  #endif
#endif
  errorCount[buf_id].index = 0;
  nanCount[buf_id].index = 0;
  flipsCount[buf_id].index = 0;
  ccCount[buf_id].index = 0;
}

/* This function is called when a shadow slice starts executing.
 * It initializes the shadow memory and sets the flags for the shadow slice.
 * */
extern "C" void fpsan_shadow_slice_start(int buf_id) {
  if (DEBUG_PROTO_C) {
    std::cout << "consumer: starting on queue:" << buf_id << "\n";
    gettimeofday(&tv1[buf_id], NULL);
  }

  if (!m_init_flag[buf_id]) {
    m_init_flag[buf_id] = true;
#ifdef TRACING
    size_t length = MAX_STACK_SIZE * sizeof(temp_entry);
    m_lock_key_map[buf_id] =
        (size_t *)mmap(0, length, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);
    assert(m_lock_key_map[buf_id] != (void *)-1);
#endif
    size_t hash_size = (HASH_TABLE_ENTRIES) * sizeof(smem_entry);
    m_shadow_memory[buf_id] = (smem_entry *)mmap(
        0, hash_size, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);
    assert(m_shadow_memory[buf_id] != (void *)-1);
    m_precision[buf_id].index = PRECISION;
#ifdef TRACING
    m_key_stack_top[buf_id].index = 1;
    m_key_counter[buf_id].index = 1;

    m_lock_key_map[buf_id][m_key_stack_top[buf_id].index] =
        m_key_counter[buf_id].index;
#ifdef CORRECTNESS
    m_prec_bits_f[buf_id].index = 23;
    m_prec_bits_d[buf_id].index = 52;
    mpfr_init2(op1_mpfr[buf_id].val, m_precision[buf_id].index);
    mpfr_init2(op2_mpfr[buf_id].val, m_precision[buf_id].index);
    mpfr_init2(res_mpfr[buf_id].val, m_precision[buf_id].index);
    mpfr_init2(temp_diff[buf_id].val, m_precision[buf_id].index);
    mpfr_init2(computed[buf_id].val, m_precision[buf_id].index);
#endif
#endif
  }
}

/* This function is called once producer finishes the execution.
 * */
extern "C" void fpsan_finish() {
  if (DEBUG_PROTO_P) {
    std::cout << "fini is set\n\n\n";
  }
  __atomic_store_n(&(fini), 1, 0);
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(thread[i], NULL);
  }
  if(DEBUG_BUF_C){
    fclose(m_errfile_c);
    fclose(m_errfile_p);
  }
  for (int i = 0; i < NUM_QUEUES; i++) {
    fclose(m_errfile[i]);
    fclose(m_fpcore[i]);
  }
}


/* This function is called once producer starts the execution.
 * */
extern "C" void fpsan_init() {
  // Remove error log, as each thread appends to this file
  remove("error*.log");
  size_t qValLen = BUF_SIZE * sizeof(double);
  if (DEBUG_PROTO_P) {
    std::cout << "NUM_QUEUES:" << NUM_QUEUES << "\n";
  }
  empty_list.set_capacity(NUM_QUEUES);
  task_queue.set_capacity(NUM_QUEUES);
  if(DEBUG_BUF_C){
    std::string filename = "error_p.log";
    m_errfile_p = fopen(filename.c_str(), "w");
    std::string filenamec = "error_cc.log";
    m_errfile_c = fopen(filenamec.c_str(), "w");
  }
  for (int i = 0; i < NUM_QUEUES; i++) {
    if (DEBUG_PROTO_P) {
      std::cout<<"empty_list: pushing:"<<i<<"\n";
    }
    std::string filename = "error" + std::to_string(i) + ".log";
    m_errfile[i] = fopen(filename.c_str(), "a+");
    m_fpcore[i] = fopen ("fpsan.fpcore","a+");
    empty_list.push(i);
    queue[i] =
        (double *)mmap(0, qValLen, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);
    assert(queue[i] != (void *)-1);
  }
  for (size_t i = 0; i < NUM_THREADS; i++) {
    pthread_create(&(thread[i]), NULL, worker, (void *)i);
  }
}

void m_set_mpfr(mpfr_t *val1, mpfr_t *val2) {
  mpfr_set(*val1, *val2, MPFR_RNDN);
}

extern "C" void fpsanx_init_mpfr_all(int buf_id, temp_entry *op, int size) {
  for(int i = 0; i<size; i++){
    mpfr_init2((op+i)->val, m_precision[buf_id].index);
  }
}

// primarily used in the LLVM IR for initializing stack metadata
extern "C" void fpsanx_init_mpfr(int buf_id, temp_entry *op) {
  mpfr_init2(op->val, m_precision[buf_id].index);
}

extern "C" void fpsanx_clear_mpfr_all(temp_entry *op, int size) {
  for(int i = 0; i<size; i++){
    mpfr_clear((op+i)->val);
  }
}

extern "C" void fpsanx_clear_mpfr(temp_entry *op) {
  mpfr_clear(op->val);
}

int m_isnan(mpfr_t real) { return mpfr_nan_p(real); }

extern "C" void fpsan_set_lib(int buf_id, double d, temp_entry *op, size_t inst_id) {
  mpfr_set_d(op->val, d, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = d;
}

void m_store_shadow_fconst(int buf_id, smem_entry *op, float f,
                           size_t inst_id, unsigned int linenumber) {

  mpfr_set_flt(op->val, f, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->error = 0;
  op->tmp_ptr = NULL;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = f;
}

void m_init_store_shadow_fconst(int buf_id, smem_entry *op,
                                               float f, size_t inst_id, 
                                               unsigned int linenumber) {
  mpfr_init2(op->val, m_precision[buf_id].index);
  m_store_shadow_fconst(buf_id, op, f, inst_id, linenumber);
}

extern "C" void fpsanx_store_tempmeta_fconst(int buf_id, temp_entry *op, float d,
                                             unsigned int linenumber) {
  mpfr_set_d(op->val, d, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = linenumber;
  op->opcode = CONSTANT;
  op->computed = d;
}

extern "C" void fpsanx_store_tempmeta_dconst(int buf_id, temp_entry *op, double d,
                                             size_t inst_id, unsigned int linenumber) {
  mpfr_set_d(op->val, d, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = d;
}

extern "C" void fpsanx_store_tempmeta_fconst_val(int buf_id, temp_entry *op, float f,
                                            size_t inst_id, unsigned int linenumber) {

  mpfr_set_flt(op->val, f, MPFR_RNDN);

#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = f;
}

extern "C" void fpsanx_store_tempmeta_dconst_val(int buf_id, temp_entry *op, double d,
                                             size_t inst_id, unsigned int linenumber) {
  mpfr_set_d(op->val, d, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = d;
}

extern "C" void m_store_tempmeta_dconst(int buf_id, temp_entry *op, double d, 
                                             size_t inst_id, unsigned int linenumber) {
  mpfr_set_d(op->val, d, MPFR_RNDN);
#ifdef TRACING
  op->lock = m_key_stack_top[buf_id].index;
  op->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  op->op1_lock = 0;
  op->op1_key = 0;
  op->op2_lock = 0;
  op->op2_key = 0;
  op->lhs = NULL;
  op->rhs = NULL;
  op->error = 0;
  op->timestamp = m_timestamp[buf_id].index++;
#endif

  op->inst_id = inst_id;
  op->opcode = CONSTANT;
  op->computed = d;
}

float m_get_float(mpfr_t mpfr_val) { return mpfr_get_flt(mpfr_val, MPFR_RNDN); }

double m_get_double(mpfr_t mpfr_val) { return mpfr_get_d(mpfr_val, MPFR_RNDN); }

long double m_get_longdouble(temp_entry *real) {
  return mpfr_get_ld(real->val, MPFR_RNDN);
}

smem_entry *m_get_shadowaddress(int buf_id, size_t address) {
  size_t addr_int = address >> 2;
  size_t index = addr_int % HASH_TABLE_ENTRIES;
  smem_entry *realAddr = m_shadow_memory[buf_id] + index;
  return realAddr;
}

extern "C" void fpsan_handle_memset(int buf_id, size_t toAddrInt, size_t size, 
                                    int val, size_t inst_id) {
  for (size_t i = 0; i < size; i++) {
    smem_entry *dst = m_get_shadowaddress(buf_id, toAddrInt + i);
    if (!dst->is_init) {
      dst->is_init = true;
      mpfr_init2(dst->val, m_precision[buf_id].index);
    }
    mpfr_set_d(dst->val, val, MPFR_RNDN);

#ifdef TRACING
    dst->error = 0;
    dst->lock = m_key_stack_top[buf_id].index;
    dst->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
    dst->tmp_ptr = 0;
#endif

    dst->is_init = true;
    dst->inst_id = inst_id;
    dst->computed = val;
    dst->opcode = CONSTANT;
  }
}

extern "C" void fpsan_handle_memcpy(int buf_id, size_t toAddrInt, size_t fromAddrInt, int size) {
  for (int i = 0; i < size; i++) {
    smem_entry *dst = m_get_shadowaddress(buf_id, toAddrInt + i);
    if (!dst->is_init) {
      dst->is_init = true;
      mpfr_init2(dst->val, m_precision[buf_id].index);
    }
    smem_entry *src = m_get_shadowaddress(buf_id, fromAddrInt + i);
    if (!src->is_init) {
      return;
    }
    m_set_mpfr(&(dst->val), &(src->val));

#ifdef TRACING
    dst->error = src->error;
    dst->lock = m_key_stack_top[buf_id].index;
    dst->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
    dst->tmp_ptr = src->tmp_ptr;
#endif

    dst->is_init = true;
    dst->inst_id = src->inst_id;
    dst->computed = src->computed;
    dst->opcode = src->opcode;
  }
}

void m_print_real(mpfr_t mpfr_val) {
  std::cout << "shadow val:";
  mpfr_out_str(stdout, 10, 15, mpfr_val, MPFR_RNDN);
}

extern "C" double fpsanx_get_real(temp_entry *res) {
  double shadowRounded = m_get_double(res->val);
  unsigned long ulpsError = m_ulpd(shadowRounded, res->computed);

  double bitsError = log2(ulpsError + 1);
  return bitsError;
}

extern "C" void fpsan_print_real(temp_entry *op, size_t numdigits) {
  std::cout << "real:";
  mpfr_out_str(stdout, 10, numdigits, op->val, MPFR_RNDN);
  std::cout << "\n";
}

/* storing metadata for constants */
extern "C" void fpsan_store_shadow_fconst(int buf_id, size_t toAddrInt, float op, size_t inst_id,
                                          unsigned int linenumber) {
  smem_entry *dest = m_get_shadowaddress(buf_id, toAddrInt);
  if (!dest->is_init) {
    dest->is_init = true;
    mpfr_init2(dest->val, m_precision[buf_id].index);
  }
  m_init_store_shadow_fconst(buf_id, dest, op, inst_id, linenumber);
  dest->is_init = true;
}

/* storing metadata for constants */
extern "C" void fpsan_store_shadow_dconst(int buf_id, size_t toAddrInt, double op, size_t inst_id,
                                          unsigned int linenumber) {
  smem_entry *dest = m_get_shadowaddress(buf_id, toAddrInt);
  if (!dest->is_init) {
    dest->is_init = true;
    mpfr_init2(dest->val, m_precision[buf_id].index);
  }
  mpfr_set_d(dest->val, op, MPFR_RNDN);

#ifdef TRACING
  dest->lock = m_key_stack_top[buf_id].index;
  dest->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  dest->error = 0;
  dest->tmp_ptr = NULL;
#endif

  dest->inst_id = inst_id;
  dest->opcode = CONSTANT;
  dest->computed = op;
}

extern "C" void fpsan_copy_return(int buf_id, temp_entry *src,
                                  temp_entry *dest) {
  if (src != NULL) {
    m_set_mpfr(&(dest->val), &(src->val));
    dest->computed = src->computed;
    dest->opcode = src->opcode;
    dest->inst_id = src->inst_id;

#ifdef TRACING
    dest->lock = m_key_stack_top[buf_id].index;
    dest->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];

    dest->op1_lock = src->op1_lock;
    dest->op1_key = src->op1_key;
    dest->lhs = src->lhs;
    dest->op2_lock = src->op2_lock;
    dest->op2_key = src->op2_key;
    dest->rhs = src->rhs;
    dest->timestamp = src->timestamp;
    int bitsError = m_update_error(buf_id, dest, dest->computed);
    dest->error = bitsError;
#endif
  }
}

extern "C" void fpsan_copy_phi(int buf_id, temp_entry *src, temp_entry *dst) {
  if (src != NULL) {
    m_set_mpfr(&(dst->val), &(src->val));
    dst->opcode = src->opcode;
    dst->computed = src->computed;
    dst->inst_id = src->inst_id;
#ifdef TRACING
    dst->op1_lock = src->op1_lock;
    dst->op2_lock = src->op2_lock;
    dst->op1_key = src->op1_key;
    dst->op2_key = src->op2_key;
    dst->lock = m_key_stack_top[buf_id].index;
    dst->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
    dst->lhs = src->lhs;
    dst->rhs = src->rhs;
    dst->timestamp = m_timestamp[buf_id].index++;
    dst->error = src->error;
#endif
  }
}

extern "C" void fpsan_store_shadow(int buf_id, size_t toAddrInt, temp_entry *src) {
  if (src == NULL) {
    std::cout << "Error !!! __set_real trying to read invalid memory\n";
    return;
  }

  smem_entry *dest = m_get_shadowaddress(buf_id, toAddrInt);
  if (!dest->is_init) {
    dest->is_init = true;
    mpfr_init2(dest->val, m_precision[buf_id].index);
  }
  
  /*copy val*/
  m_set_mpfr(&(dest->val), &(src->val));
  /*copy everything else except res key and opcode*/
  dest->is_init = true;
#ifdef TRACING
  dest->error = src->error;
  dest->lock = m_key_stack_top[buf_id].index;
  dest->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  dest->tmp_ptr = src;
#endif

  dest->inst_id = src->inst_id;
  dest->computed = src->computed;
  dest->opcode = src->opcode;
}

extern "C" void fpsan_load_shadow_f(int buf_id, size_t AddrInt, float val, 
                                    temp_entry *src, size_t inst_id) {
  if (src == NULL) {
    if (debug) {
      printf("__load_d:Error !!! __load_d trying to load from invalid stack\n");
    }
    return;
  }
  smem_entry *dest = m_get_shadowaddress(buf_id, AddrInt);
  if (!dest->is_init) {
    fpsanx_store_tempmeta_fconst_val(buf_id, src, val, inst_id, 0); // for global variables
  } else {
    #ifdef SELECTIVE
    double orig = (double) val;
    if(orig != dest->computed){
      fpsanx_store_tempmeta_fconst_val(buf_id, src, val, inst_id, 0); //for global variables
      return;
    }
    #endif

    m_set_mpfr(&(src->val), &(dest->val));
    src->inst_id = dest->inst_id;
    src->computed = dest->computed;
    src->opcode = dest->opcode;
#ifdef TRACING
    src->lock = m_key_stack_top[buf_id].index;
    src->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
    src->error = dest->error;

    /* if the temp metadata is not available (i.e, the function
       producing the value has returned), then treat the value loaded
       from the shadow memory as a constant from the perspective of
       tracing */

    if (dest->tmp_ptr != NULL &&
        m_lock_key_map[buf_id][dest->lock] == dest->key) {
      src->op1_lock = dest->tmp_ptr->op1_lock;
      src->op1_key = dest->tmp_ptr->op1_key;
      src->op2_lock = dest->tmp_ptr->op2_lock;
      src->op2_key = dest->tmp_ptr->op2_key;
      src->lhs = dest->tmp_ptr->lhs;
      src->rhs = dest->tmp_ptr->rhs;
      src->timestamp = dest->tmp_ptr->timestamp;
    } else {
      src->op1_lock = 0;
      src->op1_key = 0;
      src->op2_lock = 0;
      src->op2_key = 0;
      src->lhs = NULL;
      src->rhs = NULL;
      src->timestamp = m_timestamp[buf_id].index++;
    }
#endif
  }
}

extern "C" void fpsan_load_shadow_d(int buf_id, size_t AddrInt, double val, 
                                        temp_entry *src, size_t inst_id) {
  if (src == NULL) {
    if (debug) {
      printf("__load_d:Error !!! __load_d trying to load from invalid stack\n");
    }
    return;
  }
  smem_entry *dest = m_get_shadowaddress(buf_id, AddrInt);
  if (!dest->is_init) {
    m_store_tempmeta_dconst(buf_id, src, val, inst_id, 0); // for global variables
  } else {
      #ifdef SELECTIVE
      /* double value in the metadata space mismatches with the computed
         value */
      if(val != dest->computed){
        m_store_tempmeta_dconst(buf_id, src, val, inst_id, 0);
        return;
      }
      #endif

    m_set_mpfr(&(src->val), &(dest->val));
    src->inst_id = dest->inst_id;
    src->computed = dest->computed;
    src->opcode = dest->opcode;
#ifdef TRACING
    src->lock = m_key_stack_top[buf_id].index;
    src->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
    src->error = dest->error;

    /* if the temp metadata is not available (i.e, the function
       producing the value has returned), then treat the value loaded
       from the shadow memory as a constant from the perspective of
       tracing */

    if (dest->tmp_ptr != NULL &&
        m_lock_key_map[buf_id][dest->lock] == dest->key) {
      src->op1_lock = dest->tmp_ptr->op1_lock;
      src->op1_key = dest->tmp_ptr->op1_key;
      src->op2_lock = dest->tmp_ptr->op2_lock;
      src->op2_key = dest->tmp_ptr->op2_key;
      src->lhs = dest->tmp_ptr->lhs;
      src->rhs = dest->tmp_ptr->rhs;
      src->timestamp = dest->tmp_ptr->timestamp;
    } else {
      src->op1_lock = 0;
      src->op1_key = 0;
      src->op2_lock = 0;
      src->op2_key = 0;
      src->lhs = NULL;
      src->rhs = NULL;
      src->timestamp = m_timestamp[buf_id].index++;
    }
#endif
  }
}

void handle_math_d(int buf_id, fp_op opCode, temp_entry *op,
                   double computedResd, temp_entry *res,
                   size_t inst_id, unsigned int linenumber) {
  if (PROFILE) {
    fpcount++;
  }

  switch (opCode) {
  case SQRT:
    mpfr_sqrt(res->val, op->val, MPFR_RNDN);
    break;
  case CBRT:
    mpfr_cbrt(res->val, op->val, MPFR_RNDN);
    break;
  case FLOOR:
    mpfr_floor(res->val, op->val);
    break;
  case CEIL:
    mpfr_ceil(res->val, op->val);
    break;
  case TAN:
    mpfr_tan(res->val, op->val, MPFR_RNDN);
    break;
  case TANH:
    mpfr_tanh(res->val, op->val, MPFR_RNDN);
    break;
  case SIN:
    mpfr_sin(res->val, op->val, MPFR_RNDN);
    break;
  case SINH:
    mpfr_sinh(res->val, op->val, MPFR_RNDN);
    break;
  case COS:
    mpfr_cos(res->val, op->val, MPFR_RNDN);
    break;
  case COSH:
    mpfr_cosh(res->val, op->val, MPFR_RNDN);
    break;
  case ACOS:
    mpfr_acos(res->val, op->val, MPFR_RNDN);
    break;
  case ATAN:
    mpfr_atan(res->val, op->val, MPFR_RNDN);
    break;
  case ABS:
    mpfr_abs(res->val, op->val, MPFR_RNDN);
    break;
  case LOG:
    mpfr_log(res->val, op->val, MPFR_RNDN);
    break;
  case LOG10:
    mpfr_log10(res->val, op->val, MPFR_RNDN);
    break;
  case ASIN:
    mpfr_asin(res->val, op->val, MPFR_RNDN);
    break;
  case EXP:
    mpfr_exp(res->val, op->val, MPFR_RNDN);
    break;
  default:
    std::cout << "Error!!! Math function not supported\n\n";
    exit(1);
    break;
  }
  if (isinf(computedResd))
    infCount[buf_id].index++;
  if (computedResd != computedResd)
    nanCount[buf_id].index++;

  res->inst_id = inst_id;
  res->opcode = opCode;
  res->computed = computedResd;
#ifdef TRACING
  res->op1_lock = op->lock;
  res->op1_key = m_lock_key_map[buf_id][op->lock];
  res->op2_lock = 0;
  res->op2_key = 0;
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op;
  res->rhs = nullptr;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedResd);
#else
  res->error = 0;
#endif
#endif

  if (debug) {
    printf("handle_math_d: op:%p\n", op);
    m_print_real(op->val);
    printf("handle_math_d: res:%p\n", res);
    m_print_real(res->val);
    printf("\n");
  }
}

void m_compute(int buf_id, fp_op opCode, temp_entry *op1, temp_entry *op2,
               double computedResd, temp_entry *res, size_t inst_id, int linenumber) {
  if (PROFILE) {
    fpcount++;
  }
  switch (opCode) {
  case FADD:
    mpfr_add(res->val, op1->val, op2->val, MPFR_RNDN);
    break;

  case FSUB:
    mpfr_sub(res->val, op1->val, op2->val, MPFR_RNDN);
    break;

  case FMUL:
    mpfr_mul(res->val, op1->val, op2->val, MPFR_RNDN);
    break;

  case FDIV:
    mpfr_div(res->val, op1->val, op2->val, MPFR_RNDN);
    break;

  default:
    // do nothing
    break;
  }
  if (computedResd != computedResd)
    nanCount[buf_id].index++;
  if (debug) {
    printf("compute: op1:%p\n", op1);
    m_print_real(op1->val);
    printf("compute: op2:%p\n", op2);
    m_print_real(op2->val);
    printf("compute: res:%p\n", res);
    m_print_real(res->val);
    printf("*****\n");
  }

  res->inst_id = inst_id;
  res->opcode = opCode;
  res->computed = computedResd;
#ifdef TRACING
  res->op1_lock = op1->lock;
  res->op2_lock = op2->lock;
  res->op1_key = m_lock_key_map[buf_id][op1->lock];
  res->op2_key = m_lock_key_map[buf_id][op2->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1;
  res->rhs = op2;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedResd);
#else
  res->error = 0;
#endif
#endif
}

unsigned int m_get_exact_bits(int buf_id, double opD, int precBits,
    temp_entry *shadow) {

  mpfr_set_d(computed[buf_id].val, opD, MPFR_RNDN);

  mpfr_sub(temp_diff[buf_id].val, shadow->val, computed[buf_id].val, MPFR_RNDN);

  mpfr_exp_t exp_real = mpfr_get_exp(shadow->val);
  mpfr_exp_t exp_computed = mpfr_get_exp(computed[buf_id].val);
  mpfr_exp_t exp_diff = mpfr_get_exp(temp_diff[buf_id].val);

  if (mpfr_cmp(computed[buf_id].val, shadow->val) == 0) {
    return precBits;
  } else if (exp_real != exp_computed) {
    return 0;
  } else {
    if (mpfr_cmp_ui(temp_diff[buf_id].val, 0) != 0) {
      if (precBits < abs(exp_real - exp_diff)) {
        return precBits;
      } else {
        return abs(exp_real - exp_diff);
      }
    } else {
      return 0;
    }
  }
}

mpfr_exp_t m_get_cancelled_bits(int buf_id, double op1, double op2,
                                double res) {
  mpfr_set_d(op1_mpfr[buf_id].val, op1, MPFR_RNDN);

  mpfr_set_d(op2_mpfr[buf_id].val, op2, MPFR_RNDN);

  mpfr_set_d(res_mpfr[buf_id].val, res, MPFR_RNDN);

  mpfr_exp_t exp_op1 = mpfr_get_exp(op1_mpfr[buf_id].val);
  mpfr_exp_t exp_op2 = mpfr_get_exp(op2_mpfr[buf_id].val);
  mpfr_exp_t exp_res = mpfr_get_exp(res_mpfr[buf_id].val);

  mpfr_exp_t max_exp;
  if (mpfr_regular_p(op1_mpfr[buf_id].val) == 0 ||
      mpfr_regular_p(op2_mpfr[buf_id].val) == 0 ||
      mpfr_regular_p(res_mpfr[buf_id].val) == 0)
    return 0;

  if (exp_op1 > exp_op2)
    max_exp = exp_op1;
  else
    max_exp = exp_op2;

  if (max_exp > exp_res)
    return abs(max_exp - exp_res);
  else
    return 0;
}

unsigned int m_get_cbad(mpfr_exp_t cbits, unsigned int ebitsOp1,
                        unsigned int ebitsOp2) {
  unsigned int min_ebits;
  if (ebitsOp1 > ebitsOp2)
    min_ebits = ebitsOp2;
  else
    min_ebits = ebitsOp1;
  int badness = 1 + cbits - min_ebits;
  if (badness > 0)
    return badness;
  else
    return 0;
}

#ifdef TRACING
unsigned int m_check_cc(int buf_id, double res, int precBits,
                        temp_entry *shadowOp1, temp_entry *shadowOp2,
                        temp_entry *shadowVal) {

  // If op1 or op2 is NaR, then it is not catastrophic cancellation
  if (isnan(shadowOp1->computed) || isnan(shadowOp2->computed))
    return 0;
  // If result is 0 and it has error, then it is catastrophic cancellation
  if ((res == 0) && shadowVal->error != 0)
    return 1;

  unsigned int ebitsOp1 =
      m_get_exact_bits(buf_id, shadowOp1->computed, precBits, shadowOp1);
  unsigned int ebitsOp2 =
      m_get_exact_bits(buf_id, shadowOp2->computed, precBits, shadowOp2);
  mpfr_exp_t cbits = m_get_cancelled_bits(buf_id, shadowOp1->computed,
                                          shadowOp2->computed, res);
  unsigned int cbad = m_get_cbad(cbits, ebitsOp1, ebitsOp2);
  return cbad;
}
#endif

extern "C" void fpsan_mpfr_fadd_f(int buf_id, temp_entry *op1Idx,
                                  temp_entry *op2Idx, temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {

  float computedResD = op1Idx->computed + op2Idx->computed;
  m_compute(buf_id, FADD, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
  
#ifdef CORRECTNESS
  unsigned int cbad = 0;
  if (((op1Idx->computed < 0) && (op2Idx->computed > 0)) ||
      ((op1Idx->computed > 0) && (op2Idx->computed < 0))) {
    cbad = m_check_cc(buf_id, computedResD, m_prec_bits_f[buf_id].index, op1Idx,
        op2Idx, res);
    if (cbad > 0){
      ccCount[buf_id].index++;
    }
  }
#endif
}

extern "C" void fpsan_mpfr_fsub_f(int buf_id, temp_entry *op1Idx,
                                  temp_entry *op2Idx, temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {
  float computedResD = op1Idx->computed - op2Idx->computed;
  m_compute(buf_id, FSUB, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
  
#ifdef CORRECTNESS
  unsigned int cbad = 0;
  if (((op1Idx->computed < 0) && (op2Idx->computed < 0)) ||
      ((op1Idx->computed > 0) && (op2Idx->computed > 0))) {
    cbad = m_check_cc(buf_id, computedResD, m_prec_bits_f[buf_id].index, op1Idx,
                      op2Idx, res);
    if (cbad > 0){
      ccCount[buf_id].index++;
    }
  }
#endif
}

extern "C" void fpsan_mpfr_fmul_f(int buf_id, temp_entry *op1Idx,
                                  temp_entry *op2Idx, temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {

  float computedResD = op1Idx->computed * op2Idx->computed;
  m_compute(buf_id, FMUL, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_fdiv_f(int buf_id, temp_entry *op1Idx,
                                  temp_entry *op2Idx, temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {

  float computedResD = op1Idx->computed / op2Idx->computed;
  m_compute(buf_id, FDIV, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
  if (isinf(computedResD)){
    infCount[buf_id].index++;
  }
}

extern "C" void fpsan_mpfr_fneg(int buf_id, temp_entry *op1Idx, temp_entry *res) {

  mpfr_t zero;
  mpfr_init2(zero, m_precision[buf_id].index);
  mpfr_set_d(zero, 0, MPFR_RNDN);

  mpfr_sub(res->val, zero, op1Idx->val, MPFR_RNDN);
  mpfr_clear(zero);
#ifdef TRACING
  res->op1_lock = op1Idx->op1_lock;
  res->op2_lock = op1Idx->op2_lock;
  res->op1_key = op1Idx->op1_key;
  res->op2_key = op1Idx->op2_key;
  res->lock = op1Idx->lock;
  res->key = op1Idx->key;
  res->lhs = op1Idx->lhs;
  res->rhs = op1Idx->rhs;
  res->timestamp = op1Idx->timestamp;
  res->error = op1Idx->error;
#endif

  res->inst_id = op1Idx->inst_id;
  res->opcode = op1Idx->opcode;
  res->computed = -op1Idx->computed;
}

extern "C" void fpsan_mpfr_fadd(int buf_id, temp_entry *op1Idx,
                                temp_entry *op2Idx, temp_entry *res,
                                size_t inst_id, unsigned int linenumber) {
  double computedResD = op1Idx->computed + op2Idx->computed;
  m_compute(buf_id, FADD, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
  
#ifdef CORRECTNESS
  unsigned int cbad = 0;
  if (((op1Idx->computed < 0) && (op2Idx->computed > 0)) ||
      ((op1Idx->computed > 0) && (op2Idx->computed < 0))) {
    cbad = m_check_cc(buf_id, computedResD, m_prec_bits_d[buf_id].index, op1Idx,
                      op2Idx, res);
    if (cbad > 0){
      ccCount[buf_id].index++;
    }
  }
#endif
}

extern "C" void fpsan_mpfr_fsub(int buf_id, temp_entry *op1Idx,
                                temp_entry *op2Idx, temp_entry *res,
                                size_t inst_id, unsigned int linenumber) {

  double computedResD = op1Idx->computed - op2Idx->computed;
  m_compute(buf_id, FSUB, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);

#ifdef CORRECTNESS
  unsigned int cbad = 0;
  if (((op1Idx->computed < 0) && (op2Idx->computed < 0)) ||
      ((op1Idx->computed > 0) && (op2Idx->computed > 0))) {
    cbad = m_check_cc(buf_id, computedResD, m_prec_bits_d[buf_id].index, op1Idx,
                      op2Idx, res);
    if (cbad > 0){
      ccCount[buf_id].index++;
    }
  }
#endif
}

extern "C" void fpsan_mpfr_fmul(int buf_id, temp_entry *op1Idx,
                                temp_entry *op2Idx, temp_entry *res,
                                size_t inst_id, unsigned int linenumber) {
  double computedResD = op1Idx->computed * op2Idx->computed;
  m_compute(buf_id, FMUL, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_fdiv(int buf_id, temp_entry *op1Idx,
                                temp_entry *op2Idx, temp_entry *res,
                                size_t inst_id, unsigned int linenumber) {

  double computedResD = op1Idx->computed / op2Idx->computed;
  m_compute(buf_id, FDIV, op1Idx, op2Idx, computedResD, res, inst_id, linenumber);
  if (isinf(computedResD))
    infCount[buf_id].index++;
}

bool m_check_branch(mpfr_t *op1, mpfr_t *op2, size_t fcmpFlag) {
  bool realRes = false;
  int ret = mpfr_cmp(*op1, *op2);

  switch (fcmpFlag) {
  case 0:
    realRes = false;
    break;
  case 1: /*oeq*/
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret == 0)
        realRes = true;
    }
    break;
  case 2: /*ogt*/
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret > 0) {
        realRes = true;
      }
    }
    break;
  case 3:
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret > 0 || ret == 0) {
        realRes = true;
      }
    }
    break;
  case 4: /*olt*/
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret < 0) {
        realRes = true;
      }
    }
    break;
  case 5:
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret < 0 || ret == 0) {
        realRes = true;
      }
    }
    break;
  case 6:
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      if (ret != 0) {
        realRes = true;
      }
    }
    break;
  case 7:
    if (!m_isnan(*op1) && !m_isnan(*op2)) {
      realRes = true;
    }
    break;
  case 8:
    if (m_isnan(*op1) && m_isnan(*op2)) {
      realRes = true;
    }
    break;
  case 9:
    if (m_isnan(*op1) || m_isnan(*op2) || ret == 0)
      realRes = true;
    break;
  case 10:
    if (m_isnan(*op1) || m_isnan(*op2) || ret > 0)
      realRes = true;
    break;
  case 11:
    if (m_isnan(*op1) || m_isnan(*op2) || ret >= 0)
      realRes = true;
    break;
  case 12:
    if (m_isnan(*op1) || m_isnan(*op2) || ret < 0)
      realRes = true;
    break;
  case 13:
    if (m_isnan(*op1) || m_isnan(*op2) || ret <= 0)
      realRes = true;
    break;
  case 14:
    if (m_isnan(*op1) || m_isnan(*op2) || ret != 0) {
      realRes = true;
    }
    break;
  case 15:
    realRes = true;
    break;
  }
  return realRes;
}

extern "C" bool fpsan_check_branch_f(int buf_id, temp_entry *op1,
                                     temp_entry *op2, size_t fcmpFlag, 
                                     bool computedRes, size_t inst_id,
                                     size_t lineNo) {

  bool realRes = m_check_branch(&(op1->val), &(op2->val), fcmpFlag);
  if (realRes != computedRes) {
    flipsCount[buf_id].index++;
#ifdef DEBUG_ERROR
    if(error_map[buf_id].count(inst_id) == 0){
      error_map[buf_id].insert(std::pair<size_t, int>(inst_id, flipsCount[buf_id].index));
      fprintf(m_errfile[buf_id], "Total branch flips found %zd\n", lineNo);
    }
#endif
  }
  return realRes;
}

extern "C" bool fpsan_check_branch_d(int buf_id, temp_entry *op1,
                                     temp_entry *op2, size_t fcmpFlag, 
                                     bool computedRes, size_t inst_id,
                                     size_t lineNo) {

  bool realRes = m_check_branch(&(op1->val), &(op2->val), fcmpFlag);
  if (realRes != computedRes) {
    flipsCount[buf_id].index++;
#ifdef DEBUG_ERROR
    if(error_map[buf_id].count(inst_id) == 0){
      error_map[buf_id].insert(std::pair<size_t, int>(inst_id, flipsCount[buf_id].index));
      fprintf(m_errfile[buf_id], "Total branch flips found %zd\n", lineNo);
    }
#endif
  }
  return realRes;
}

std::string m_get_string_opcode(size_t opcode) {

  switch (opcode) {
  case FADD:
    return "FADD";
  case FMUL:
    return "FMUL";
  case FSUB:
    return "FSUB";
  case FDIV:
    return "FDIV";
  case CONSTANT:
    return "CONSTANT";
  case SQRT:
    return "SQRT";
  case CBRT:
    return "CBRT";
  case FMA:
    return "FMA";
  case FLOOR:
    return "FLOOR";
  case TAN:
    return "TAN";
  case SIN:
    return "SIN";
  case COS:
    return "COS";
  case ATAN:
    return "ATAN";
  case ABS:
    return "ABS";
  case LOG:
    return "LOG";
  case ASIN:
    return "ASIN";
  case EXP:
    return "EXP";
  case POW:
    return "POW";
  case FMOD:
    return "FMOD";
  default:
    return "Unknown";
  }
}

#ifdef TRACING
int m_get_depth(int buf_id, temp_entry *current) {
  int depth = 0;
  m_expr[buf_id].push_back(current);
  int level;
  while (!m_expr[buf_id].empty()) {
    level = m_expr[buf_id].size();
    std::cout << "\n";
    while (level > 0) {
      temp_entry *cur = m_expr[buf_id].front();
      if (cur == NULL) {
        return depth;
      }
      if (m_lock_key_map[buf_id][cur->op1_lock] != cur->op1_key) {
        return depth;
      }
      if (m_lock_key_map[buf_id][cur->op2_lock] != cur->op2_key) {
        return depth;
      }
      if (cur->lhs != NULL) {
        if (cur->lhs->timestamp < cur->timestamp) {
          m_expr[buf_id].push_back(cur->lhs);
        }
      }
      if (cur->rhs != NULL) {
        if (cur->rhs->timestamp < cur->timestamp) {
          m_expr[buf_id].push_back(cur->rhs);
        }
      }
      m_expr[buf_id].pop_front();
      level--;
    }
    depth++;
  }
  return depth;
}
#endif

extern "C" void fpsan_func_init(int buf_id) {
#ifdef TRACING
  m_key_stack_top[buf_id].index++;
  m_key_counter[buf_id].index++;
  m_lock_key_map[buf_id][m_key_stack_top[buf_id].index] = m_key_counter[buf_id].index;
#endif
}

extern "C" void fpsan_func_exit(int buf_id) {
#ifdef TRACING
  m_lock_key_map[buf_id][m_key_stack_top[buf_id].index] = 0;
  m_key_stack_top[buf_id].index--;
#endif
}

unsigned long m_ulpd(double x, double y) {
  if (x == 0)
    x = 0; // -0 == 0
  if (y == 0)
    y = 0; // -0 == 0

  if (x != x)
    return ULLONG_MAX - 1; // Maximum error
  if (y != y)
    return ULLONG_MAX - 1; // Maximum error

  long long xx = *((long long *)&x);
  xx = xx < 0 ? LLONG_MIN - xx : xx;

  long long yy = *((long long *)&y);
  yy = yy < 0 ? LLONG_MIN - yy : yy;
  return xx >= yy ? xx - yy : yy - xx;
}

int m_update_error(int buf_id, temp_entry *real, double computedVal) {
  double shadowRounded = m_get_double(real->val);
  unsigned long ulpsError = m_ulpd(shadowRounded, computedVal);

  double bitsError = log2(ulpsError + 1);

  if (bitsError > ERRORTHRESHOLD){
    errorCount[buf_id].index++;
#ifdef DEBUG_ERROR
#ifdef TRACING
    if(error_map[buf_id].count(real->inst_id) == 0){
      error_map[buf_id].insert(std::pair<size_t, int>(real->inst_id, (int)bitsError) );
      fprintf(m_errfile[buf_id], "Error above %d (inst_id: error:)\n", ERRORTHRESHOLD);
      fprintf(m_errfile[buf_id], "(%zd: %d)\n",
          real->inst_id, (int)bitsError);
    }
#endif
#endif
  }

  if (debugerror) {
    std::cout << "\nThe shadow value is ";
    m_print_real(real->val);
    if (computedVal != computedVal) {
      std::cout << ", but NaN was computed.\n";
    } else {
      std::cout << ", but ";
      printf("%e", computedVal);
      std::cout << " was computed.\n";
      std::cout << "m_update_error: computedVal:" << computedVal << "\n";
    }
    printf("%f bits error (%lu ulps)\n", bitsError, ulpsError);
    std::cout << "****************\n\n";
  }
  return bitsError;
}

extern "C" void fpsan_get_error(int buf_id, void *Addr, double computed) {
  size_t AddrInt = (size_t)Addr;
  smem_entry *dest = m_get_shadowaddress(buf_id, AddrInt);

  double shadowRounded = m_get_double(dest->val);
  unsigned long ulpsError = m_ulpd(shadowRounded, computed);

  printf("getError real:");
  m_print_real(dest->val);
  printf("\n");
  printf("getError computed: %e", computed);
  printf("\n");
  double bitsError = log2(ulpsError + 1);
  fprintf(m_errfile[buf_id], "computed:%e real:%e Error: %lu ulps (%lf bits)\n",
          computed, shadowRounded, ulpsError, bitsError);
}

/* Math library functions */
extern "C" void fpsan_mpfr_sqrt(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = sqrt(op1->computed);
  handle_math_d(buf_id, SQRT, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_cbrt(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = cbrt(op1->computed);
  handle_math_d(buf_id, CBRT, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_sqrtf(int buf_id, temp_entry *op1, temp_entry *res,
                                 size_t inst_id,
                                 unsigned int linenumber) {

  double computedRes = sqrtf(op1->computed);
  handle_math_d(buf_id, SQRT, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_floor(int buf_id, temp_entry *op1, temp_entry *res,
                                 size_t inst_id,
                                 unsigned int linenumber) {

  double computedRes = floor(op1->computed);
  handle_math_d(buf_id, FLOOR, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_tanh(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = tanh(op1->computed);
  handle_math_d(buf_id, TANH, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_tan(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = tan(op1->computed);
  handle_math_d(buf_id, TAN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_acos(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = acos(op1->computed);
  handle_math_d(buf_id, ACOS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_cosh(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = cosh(op1->computed);
  handle_math_d(buf_id, COSH, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_cos(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = cos(op1->computed);
  handle_math_d(buf_id, COS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_cosf(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  float computedRes = cosf(op1->computed);
  handle_math_d(buf_id, COS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_llvm_fma_f643(int buf_id, temp_entry *op1, temp_entry *op2, 
                               temp_entry *op3,
                               temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = fma(op1->computed, op2->computed, op3->computed);
  mpfr_fma(res->val, op1->val, op2->val, op3->val, MPFR_RNDN);
  res->inst_id = inst_id;
  res->opcode = FMA;
  res->computed = computedRes;

#ifdef TRACING
  res->op1_lock = op1->lock;
  res->op1_key = m_lock_key_map[buf_id][op1->lock];
  res->op2_lock = op2->lock;
  res->op2_key = m_lock_key_map[buf_id][op2->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1;
  res->rhs = op2;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_llvm_cos_f64(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = cos(op1->computed);
  handle_math_d(buf_id, COS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_atan(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = atan(op1->computed);
  handle_math_d(buf_id, ATAN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_llvm_ceil(int buf_id, temp_entry *op1,
                                     temp_entry *res,
                                     size_t inst_id, unsigned int linenumber) {

  double computedRes = ceil(op1->computed);
  handle_math_d(buf_id, CEIL, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_llvm_floor_d(int buf_id, temp_entry *op1Idx,
                                      temp_entry *res,
                                      size_t inst_id, unsigned int linenumber) {

  double computedRes = floor(op1Idx->computed);
  handle_math_d(buf_id, FLOOR, op1Idx, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_llvm_floor_f(int buf_id, temp_entry *op1Idx,
                                      temp_entry *res,
                                      size_t inst_id, unsigned int linenumber) {

  double computedRes = floor(op1Idx->computed);
  handle_math_d(buf_id, FLOOR, op1Idx, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_exp(int buf_id, temp_entry *op1Idx, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = exp(op1Idx->computed);
  handle_math_d(buf_id, EXP, op1Idx, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_expf(int buf_id, temp_entry *op1Idx, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  float computedRes = expf(op1Idx->computed);
  handle_math_d(buf_id, EXP, op1Idx, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_ldexp2(int buf_id, temp_entry *op1Idx, int op2d,
                                  temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {

  // op1*2^(op2)
  mpfr_t exp;
  mpfr_init2(exp, m_precision[buf_id].index);
  mpfr_set_si(exp, op2d, MPFR_RNDN);
  mpfr_exp2(res->val, exp, MPFR_RNDN);
  mpfr_mul(res->val, op1Idx->val, res->val, MPFR_RNDN);

  mpfr_clear(exp);

  double computedRes = ldexp(op1Idx->computed, op2d);
  res->inst_id = inst_id;
  res->opcode = LDEXP;
  res->computed = computedRes;

#ifdef TRACING
  res->op1_lock = op1Idx->lock;
  res->op1_key = m_lock_key_map[buf_id][op1Idx->lock];
  res->op2_lock = 0;
  res->op2_key = 0;
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1Idx;
  res->rhs = NULL;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_fmod2(int buf_id, temp_entry *op1Idx,
                                 temp_entry *op2Idx, temp_entry *res,
                                 size_t inst_id,
                                 unsigned int linenumber) {

  mpfr_fmod(res->val, op1Idx->val, op2Idx->val, MPFR_RNDN);

  double computedRes = fmod(op1Idx->computed, op2Idx->computed);
  res->opcode = FMOD;
  res->computed = computedRes;
  res->inst_id = inst_id;
#ifdef TRACING
  res->op1_lock = op1Idx->lock;
  res->op1_key = m_lock_key_map[buf_id][op1Idx->lock];
  res->op2_lock = op2Idx->lock;
  res->op2_key = m_lock_key_map[buf_id][op2Idx->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1Idx;
  res->rhs = op2Idx;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_atan22(int buf_id, temp_entry *op1, temp_entry *op2,
                                  temp_entry *res,
                                  size_t inst_id, unsigned int linenumber) {

  mpfr_atan2(res->val, op1->val, op2->val, MPFR_RNDN);

  double computedRes = atan2(op1->computed, op2->computed);
  res->opcode = ATAN2;
  res->computed = computedRes;
  res->inst_id = inst_id;

#ifdef TRACING
  res->op1_lock = op1->lock;
  res->op1_key = m_lock_key_map[buf_id][op1->lock];
  res->op2_lock = op2->lock;
  res->op2_key = m_lock_key_map[buf_id][op2->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1;
  res->rhs = op2;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_hypot2(int buf_id, temp_entry *op1, temp_entry *op2,
                                  temp_entry *res, 
                                  size_t inst_id, unsigned int linenumber) {

  mpfr_hypot(res->val, op1->val, op2->val, MPFR_RNDN);

  double computedRes = hypot(op1->computed, op2->computed);
  res->opcode = HYPOT;
  res->computed = computedRes;
  res->inst_id = inst_id;
#ifdef TRACING
  res->op1_lock = op1->lock;
  res->op1_key = m_lock_key_map[buf_id][op1->lock];
  res->op2_lock = op2->lock;
  res->op2_key = m_lock_key_map[buf_id][op2->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1;
  res->rhs = op2;
  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_pow2(int buf_id, temp_entry *op1, temp_entry *op2,
                                temp_entry *res,
                                size_t inst_id, unsigned int linenumber) {

  mpfr_pow(res->val, op1->val, op2->val, MPFR_RNDN);

  double computedRes = pow(op1->computed, op2->computed);
  res->opcode = POW;
  res->computed = computedRes;
  res->inst_id = inst_id;
#ifdef TRACING
  res->op1_lock = op1->lock;
  res->op1_key = m_lock_key_map[buf_id][op1->lock];
  res->op2_lock = op2->lock;
  res->op2_key = m_lock_key_map[buf_id][op2->lock];
  res->lock = m_key_stack_top[buf_id].index;
  res->key = m_lock_key_map[buf_id][m_key_stack_top[buf_id].index];
  res->lhs = op1;
  res->rhs = op2;

  res->timestamp = m_timestamp[buf_id].index++;
#ifdef CORRECTNESS
  res->error  = m_update_error(buf_id, res, computedRes);
#else
  res->error = 0;
#endif
#endif
}

extern "C" void fpsan_mpfr_llvm_fabs(int buf_id, temp_entry *op1,
                                     temp_entry *res,
                                     size_t inst_id, unsigned int linenumber) {

  double computedRes = fabs(op1->computed);
  handle_math_d(buf_id, ABS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_abs(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = abs(op1->computed);
  handle_math_d(buf_id, ABS, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_log10(int buf_id, temp_entry *op1, temp_entry *res,
                                 size_t inst_id,
                                 unsigned int linenumber) {

  double computedRes = log10(op1->computed);
  handle_math_d(buf_id, LOG10, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_log(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = log(op1->computed);
  handle_math_d(buf_id, LOG, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_sinh(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = sinh(op1->computed);
  handle_math_d(buf_id, SIN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_sin(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = sin(op1->computed);
  handle_math_d(buf_id, SIN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_sinf(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  float computedRes = sinf(op1->computed);
  handle_math_d(buf_id, SIN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_llvm_sin_f64(int buf_id, temp_entry *op1, temp_entry *res,
                               size_t inst_id,
                               unsigned int linenumber) {

  double computedRes = sin(op1->computed);
  handle_math_d(buf_id, SIN, op1, computedRes, res, inst_id, linenumber);
}

extern "C" void fpsan_mpfr_asin(int buf_id, temp_entry *op1, temp_entry *res,
                                size_t inst_id,
                                unsigned int linenumber) {

  double computedRes = asin(op1->computed);
  handle_math_d(buf_id, ASIN, op1, computedRes, res, inst_id, linenumber);
}
