#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
//#include "dtt.h"
//#include <sys/param.h>
//#include <sys/cpuset.h>
//#define EQ_MAX 256 
//#define EQ_MAX 512
#ifdef PFM
#ifndef PFM_DEFINED
int pfm_fd[2];
long long int PFM_start, PFM_end, totalWaitingCycle, totalDTTCycle, PFM_DTT_start;
int totalDTTs;
#define PFM_DEFINED 1
#ifdef PFM4
//struct perf_event_desc_t *fds;
uint64_t pd[2][1];
#else
pfarg_ctx_t pfm_ctx[2];
pfarg_load_t load[2];
pfmlib_input_param_t inp[2];
pfmlib_output_param_t outp[2];
pfarg_pmd_t pd[2][1]; 
pfarg_pmc_t pc[2][1];
#endif
#endif
#endif

#define EQ_MAX 1024
//#define EQ_MAX 10240
#ifdef DTT_THRESHOLD
int dtt_threshold = DTT_THRESHOLD;
#endif
dtt_event EQ[EQ_MAX];

volatile int EQ_head;
volatile int EQ_tail;
int cancel_flag=0;

//#ifdef SINGLE_THREAD
//dtt_event SINGLE_THREAD_EVENT;
//#endif
volatile int in_barrier;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef TIME
#include <sys/time.h>
struct timeval timestamp;
#endif

#ifdef PFM
#include <unistd.h>
#include <linux/unistd.h>
long long DTT_delay,enqueue;
int tstores;
#endif

#ifdef DTT_THRESHOLD
int epoch = 1024;
#ifndef DTT_DISABLE
#define DTT_DISABLE 500
#endif
#endif

void tstore(dtt_value *target, dtt_value new_value, int size, dtt_state *entry, void *args, void (*func)(void *x))
{
  dtt_value old_value;
  dtt_event *dtt_instance;
  int i = 0;
  old_value = *target;
#ifdef PFM
#ifdef PFM4
  uint64_t values[3];

#endif
  tstores++;
#endif

#ifdef DTT_THRESHOLD
    if(entry->counter >= dtt_threshold /*|| entry->cancel == 1*/)
    {
    #ifdef MFENCE
      _update_var(entry->state, 0);
    #else
      entry->state = 0;
    #endif
      return;
    }
#endif

#ifdef OLDTSTORE
  for(i=0;i<size;i++)
  {
    if(old_value.c[i] != new_value.c[i])
#else
    if(!(!memcmp(target, &new_value, size)))
#endif
    {

#ifdef PFM
/* #ifndef PFM4
  pfm_read_pmds(pfm_fd[0], pd[0], 1);
  enqueue = pd[0][0].reg_value;
 #else
  uint64_t values[3];
  read(pfm_fd[0], values, sizeof(values));
  enqueue = values[0];
 #endif*/
#endif
      dtt_instance = dtt_event_create(entry, target, args, func);

#ifndef DTT_POLLING
#ifndef SINGLE_THREAD
      if(dtt_instance == &EQ[EQ_head]) // Only spawn when there is no thread.
#endif
      {
        int error;
    #ifdef MFENCE
          _update_var(entry->state, 2);
    #else
          entry->state = 2;
    #endif

    #ifndef SINGLE_THREAD
        #ifdef DTT_TAIL_RECURSION
        while((error = pthread_create(&entry->thread, NULL, dtt,(void *)dtt_instance)));
        pthread_detach(entry->thread);
        #else
        while((error = pthread_create(&dtt_instance->thread, NULL, func,(void *)dtt_instance)));
        pthread_detach(dtt_instance->thread);
        #endif
    #else
       #ifdef PFM
         totalDTTs++;
         #ifndef PFM4
         pfm_read_pmds(pfm_fd[0], pd[0], 1);
         PFM_DTT_start = pd[0][0].reg_value;
         #else
         //uint64_t values[3];
         read(pfm_fd[0], values, sizeof(values));
         PFM_DTT_start = values[0];
         #endif
       #endif
         func(dtt_instance);
         entry->state = 1;
         dtt_instance->valid = 0;
       #ifdef PFM
         #ifndef PFM4
          pfm_read_pmds(pfm_fd[0], pd[0], 1);
          //  fprintf(stderr, "total cycles %"PRIu64"\n", pd[1][0].reg_value);
          totalDTTCycle +=(pd[0][0].reg_value-PFM_DTT_start);
         #else
          //uint64_t values[3];
          read(pfm_fd[0], values, sizeof(values));
          //  read(pfm_fd[0], pd[0], sizeof(pd[0]));
          totalDTTCycle +=(values[0]-PFM_DTT_start);
         #endif
       #endif
    #endif
      }
#endif
#ifndef NOTSTORE
#ifdef OLDTSTORE
      break;
    }
#endif
  }
#endif
  return;
}

dtt_state *dtt_state_create()
{
  dtt_state *temp = (dtt_state *)calloc(1,sizeof(dtt_state));
  temp->cancel = 1;
  return temp;
}

dtt_event *dtt_event_create(dtt_state *state, dtt_value *target, void *args, void (*func)(void *x))
{
  dtt_event *dtt_arg;
//#ifdef DTT_POLLING
  int new_EQ_tail;
//#endif
  #ifndef NODTT
  #ifdef DTT_POLLING
    if(state->state < 2)
    {
    #ifdef MFENCE
        _update_var(state->state, 2);
    #else
        state->state = 2;
    #endif
    }
  #endif
  #else
        state->state = 0;  
  #endif
  dtt_arg = &EQ[EQ_tail];
#ifndef NODTT
  new_EQ_tail = (EQ_tail+1)%EQ_MAX;

#ifndef DTT_POLLING
//  pthread_mutex_lock( &list_mutex ); 
/*  while(EQ[new_EQ_tail].valid)
  {
  #ifdef XIP
   dtt_XIP(state);
  #endif
  ;
  }*/
//  pthread_mutex_unlock( &list_mutex ); 
#endif
#ifdef DTT_POLLING
  while(dtt_arg->valid);
#endif
#endif
  dtt_arg->state = state;
  dtt_arg->target = target;
  dtt_arg->func = func;
  dtt_arg->args = args;
#ifndef DTT_POLLING
  pthread_mutex_lock( &list_mutex ); 
#endif
  dtt_arg->valid = 1;
#ifndef DTT_POLLING
  pthread_mutex_unlock( &list_mutex ); 
#endif
#ifndef NODTT
#ifdef MFENCE
  _update_var(EQ_tail, (EQ_tail+1)%EQ_MAX);
#else
  EQ_tail = new_EQ_tail;
#endif
#endif
  return dtt_arg;
}

#ifdef MFENCE
dtt_event* dtt_return(dtt_state *state, dtt_value return_value, dtt_event *event)
#else
dtt_event* dtt_return(volatile dtt_state *state, dtt_value return_value, dtt_event *event)
#endif
{
  dtt_event *ret;
  ret=NULL;

#ifdef MFENCE
  _update_var(state->return_value, return_value);
#else
  state->return_value = return_value;
#endif
/*
#ifndef PARALLEL_DTT
#ifndef DTT_POLLING
  if(EQ_head!=EQ_tail)
#endif
  {
    #ifdef MFENCE
    _update_var(EQ_head, (EQ_head+1)%EQ_MAX);
    #else
    EQ_head = (EQ_head+1)%EQ_MAX;
    #endif
  }
#endif
*/
#ifndef SINGLE_THREAD
#ifndef DTT_POLLING
#ifndef DTT_TAIL_RECURSION
//  if(EQ_head == EQ_tail /*|| in_barrier == 1*/) // This is the last task
  if(EQ[EQ_head].valid == 0) // This is the last task
  {
//    EQ_head = EQ_tail;
    ret=NULL;
  }
  else
  {
    ret = &EQ[EQ_head];
//    event->next = NULL;
  }
  
  if(ret != NULL)
  {
    int error;
    if(error = pthread_create(&ret->state->thread, NULL, ret->func,(void *)ret))
    {
  pthread_mutex_lock( &list_mutex ); 
        event->valid = 0;
  pthread_mutex_unlock( &list_mutex ); 
        ret->func(ret);
  pthread_mutex_lock( &list_mutex ); 
        ret->valid = 0;
  pthread_mutex_unlock( &list_mutex ); 
        return NULL;
    }
    pthread_detach(&ret->state->thread);
  }
  else
  {
      if(in_barrier == 1)
      {
#ifdef MFENCE
        _update_var(state->state, 0);
#else
        state->state = 0;
#endif
      }
      else
      {
#ifdef MFENCE
         _update_var(state->state, 1);
#else
        state->state = 1;
#endif
      }
  }
#endif
#endif
#endif
#ifndef DTT_POLLING
  pthread_mutex_lock( &list_mutex ); 
#endif
  event->valid = 0;
#ifndef DTT_POLLING
  pthread_mutex_unlock( &list_mutex ); 
#endif
  return ret;
}

dtt_event* dtt_cancel(dtt_state *state)
{
  dtt_event *ret =NULL;
//  pthread_kill(EQ[EQ_head].thread, 0);
  cancel_flag = 1;
#ifdef MFENCE
    _update_var(EQ_head, EQ_tail);
#else
  EQ_head = EQ_tail;
#endif
//  state->state = 0;
  return ret;
}

#ifdef DTT_POLLING
void *dtt(void *x)
{
#ifdef MFENCE
  dtt_state *state = (dtt_state *)x;
#else
  volatile dtt_state *state = (dtt_state *)x;
#endif
  dtt_event *current =NULL;
  dtt_event *old =NULL;
#ifdef PREFETCH
  double *tmp,tmp2=0.0;
#endif
  int i;
  cpu_set_t myset;
#ifdef SMT
  CPU_SET(5, &myset);
#else
#ifdef SMP
#ifdef CORE2
  CPU_SET(3, &myset);
#else
  CPU_SET(8, &myset);
#endif
#else
  CPU_SET(6, &myset);
#endif
#endif
  sched_setaffinity(0, sizeof(myset), &myset);
#ifdef PFM
  PFM_init(1);
#ifdef PFM4
  uint64_t values[3];
#endif
#endif
  in_barrier = 0;
  while(1)
  {
    while(EQ_head != EQ_tail || EQ[EQ_head].valid)
//    while(EQ[EQ_head].valid)
    {
#ifdef PFM
        totalDTTs++;
        #ifndef PFM4
        pfm_read_pmds(pfm_fd[1], pd[1], 1);
        PFM_DTT_start = pd[1][0].reg_value;
        #else
        read(pfm_fd[1], values, sizeof(values));
        PFM_DTT_start = values[0];
        #endif
#endif
        current = &EQ[EQ_head];

//        pthread_mutex_lock( &list_mutex );
        if(current && current->state)
        {
        current->valid = 2;
#ifdef MFENCE
          _update_var(current->state->state, 2);
#else
          current->state->state = 2;
#endif
          old=current;
//        pthread_mutex_unlock( &list_mutex );
          current->func(current);
          EQ_head = (EQ_head+1)%EQ_MAX;
        }
//        current->valid = 0;
#ifdef PFM
 #ifndef PFM4
  pfm_read_pmds(pfm_fd[1], pd[1], 1);
//  fprintf(stderr, "total cycles %"PRIu64"\n", pd[1][0].reg_value);
  totalDTTCycle +=(pd[1][0].reg_value-PFM_DTT_start);
 #else
        read(pfm_fd[1], values, sizeof(values));
  totalDTTCycle +=(values[0]-PFM_DTT_start);
 #endif
#endif
    }
    if(old && old->state)
    {
        if(in_barrier)
        {
#ifdef MFENCE
          _update_var(old->state->state, 0);
#else
          old->state->state = 0;
#endif
        }
        else
        {
#ifdef MFENCE
          _update_var(old->state->state, 1);
#else
          old->state->state = 1;
#endif
        }
        old =NULL;
    }
  }
  pthread_exit(NULL);
  return NULL;
}
#else
void *dtt(void *x)
{
#ifdef MFENCE
  dtt_event *current = (dtt_event *)x;
  dtt_event *old = current;
#else
  volatile dtt_event *current = (dtt_event *)x;
  volatile dtt_event *old = current;
#endif
  int i;
  cpu_set_t myset;
//  CPU_SET(4, &myset);
#ifdef SMT
  CPU_SET(5, &myset);
#else
  CPU_SET(6, &myset);
#endif
  sched_setaffinity(0, sizeof(myset), &myset);
  while(EQ_head != EQ_tail)
  {
//        pthread_mutex_lock( &list_mutex );

#ifdef MFENCE
          _update_var(current->state->state, 2);
#else
        current->state->state = 2;
#endif
        current->func(current);
        old=current;
        current = &EQ[EQ_head];
        if(in_barrier)
          break;
  }
  if(in_barrier)
  {
#ifdef MFENCE
    _update_var(old->state->state, 0);
#else
    old->state->state = 0;
#endif
  }
  else
  {
#ifdef MFENCE
    _update_var(old->state->state, 1);
#else
    old->state->state = 1;
#endif
  }
//  pthread_mutex_unlock( &list_mutex );
  pthread_exit(NULL);
  return;
}
#endif

#ifdef MFENCE
void dtt_barrier(dtt_state *entry)
#else
void dtt_barrier(volatile dtt_state *entry)
#endif
{
#ifdef PFM4
 uint64_t values[3];
#endif
#ifdef PARALLEL_DTT
  dtt_event *current;
#endif
#ifndef SINGLE_THREAD
  if(entry && entry->state == 2)
  {
#ifdef DTT_THRESHOLD
  entry->stalled++;
#endif
#ifdef PFM
//  int output = 0;
#ifndef PFM4
  pfm_read_pmds(pfm_fd[0], pd[0], 1);
  PFM_start = pd[0][0].reg_value;
#else
//  read(pfm_fd[0], &PFM_start, sizeof(PFM_start));
  read(pfm_fd[0], values, sizeof(values));
  PFM_start = values[0];
#endif
#endif
  #ifdef PARALLEL_DTT
    while(entry->state == 2)
    {
        current = &EQ[EQ_head];
        if(EQ_head!=EQ_tail)
        {
        #ifdef MFENCE
          _update_var(EQ_head, (EQ_head+1)%EQ_MAX);
        #else
          EQ_head = (EQ_head+1)%EQ_MAX;
        #endif
        }

//        pthread_mutex_lock( &list_mutex );
        if(current->state)
        {
        #ifdef MFENCE
          _update_var(current->state->state, 2);
        #else
          current->state->state = 2;
        #endif
//          old=current;
//        pthread_mutex_unlock( &list_mutex );
          current->func(current);
          if(EQ_head==EQ_tail)
          {
        #ifdef MFENCE
            _update_var(current->state->state, 1);
        #else
            current->state->state = 1;
        #endif
          }
        }
    }
  #else
  while(entry->state == 2)
  {
    #ifdef DTT_THRESHOLD
//      entry->counter++;
    #ifdef MIGRATION
    else
      entry->state = 1;
    #endif
    #endif
  }
  #endif
//    in_barrier = 0;    
  #ifdef DTT_CANCEL
  #ifndef PARALLEL_DTT
    if(EQ_head != EQ_tail)
    {
      #ifdef MFENCE
      _update_var(entry->state, 0);
      #else
      entry->state = 0;
      #endif
      #ifdef MFENCE
      _update_var(EQ_head, EQ_tail);
      #else
      EQ_head = EQ_tail;
      #endif
    }
  #endif
  #endif
#ifdef PFM
#ifndef PFM4
  pfm_read_pmds(pfm_fd[0], pd[0], 1);
  totalWaitingCycle +=(pd[0][0].reg_value-PFM_start);
//  DTT_delay += pd[0][0].reg_value - enqueue;
  enqueue = 0;
#else
  read(pfm_fd[0], values, sizeof(values));
  totalWaitingCycle +=(values[0]-PFM_start);
//  DTT_delay += values[0] - enqueue;
  enqueue = 0;
#endif
#endif
  }
#endif
#ifdef DTT_THRESHOLD
  entry->called++;
#ifdef DTT_RESET
  if(entry->called == 10*epoch)
  {
   entry->called = 0;
   if(entry->counter)
    entry->counter = 0;
  }
#endif
  if(entry->called & 0x400 == 0)
  {
   // more than half of them has to be stalled.
   if(entry->stalled >  DTT_DISABLE)
   {
    entry->counter = dtt_threshold*2;
    entry->called = 0;
   }
    entry->stalled = 0; // reset the stall counter
  }
#endif
}

#ifdef MFENCE
void dtt_XIP(dtt_state *entry)
#else
void dtt_XIP(volatile dtt_state *entry)
#endif
{
  dtt_event *current;
#ifdef XIP
  XIPing = 1;
#endif
  current = &EQ[EQ_head];
  if(EQ_head!=EQ_tail)
  {
    #ifdef MFENCE
      _update_var(EQ_head, (EQ_head+1)%EQ_MAX);
    #else
      EQ_head = (EQ_head+1)%EQ_MAX;
    #endif
  }
  if(current->state)
  {
    #ifdef MFENCE
      _update_var(current->state->state, 2);
    #else
      current->state->state = 2;
    #endif
      current->func(current);
      if(EQ_head==EQ_tail)
      {
        #ifdef MFENCE
          _update_var(current->state->state, 1);
        #else
          current->state->state = 1;
        #endif
      }
  }
#ifdef XIP
  XIPing = 0;
#endif
}
/*
void dtt_end(volatile dtt_state *entry)
{
 if(entry->counter < 0) // main thread takes longer
  dtt_threshold = dtt_threshold << 1;
// else
//  dtt_threshold = dtt_threshold >> 1; // dtt takes longer
}
*/
#ifdef PFM
void PFM_init(int thread)
{
  int ret;
  #ifdef PFM4
  struct perf_event_attr attr;
  #endif
//  if(thread == 0)
  {
    ret = pfm_initialize();  
#ifndef PFM4
    if (ret != PFMLIB_SUCCESS)
#else
    if (ret != PFM_SUCCESS)
#endif    
     fprintf(stderr,"Cannot initialize library: %s\n", pfm_strerror(ret));
  }
//  pfm_find_event("CPU_CYCLES", &inp[thread].pfp_events[0]);
//  pfm_find_event("DATA_CACHE_MISSES", &inp.pfp_events[0]);
//  pfm_find_event("INSTRUCTION_CACHE_MISSES", &inp.pfp_events[0]);
//  pfm_find_event("INSTRUCTIONS_RETIRED", &inp.pfp_events[0]);
//  pfm_find_event("STORES", &inp[thread].pfp_events[0]);
//  pfm_find_event("RETIRED_INSTRUCTIONS", &inp[thread].pfp_events[0]);
#ifndef PFM4
//  if (pfm_get_cycle_event(&inp[thread].pfp_events[0]) != PFMLIB_SUCCESS)
//   fprintf(stderr,"cannot find cycle event\n");
  inp[thread].pfp_dfl_plm = PFM_PLM3; 
  inp[thread].pfp_event_count = 1;
  pfm_fd[thread] = pfm_create_context(&pfm_ctx[thread], NULL, NULL, 0);
  if(pfm_fd[thread] == -1)
   fprintf(stderr,"Can't create PFM context\n");
  ret = pfm_dispatch_events(&inp[thread], NULL, &outp[thread], NULL);
  if (ret != PFMLIB_SUCCESS)
   fprintf(stderr,"cannot configure events\n");
  pd[thread][0].reg_num = outp[thread].pfp_pmds[0].reg_num;
  pc[thread][0].reg_num = outp[thread].pfp_pmcs[0].reg_num;
  pc[thread][0].reg_value = outp[thread].pfp_pmcs[0].reg_value;
  pfm_write_pmcs(pfm_fd[thread], pc[thread], outp[thread].pfp_pmc_count);
  pfm_write_pmds(pfm_fd[thread], pd[thread], outp[thread].pfp_pmd_count);
//  load[thread].load_pid = getpid();
  load[thread].load_pid = (pid_t)syscall(__NR_gettid);
//  fprintf(stderr, "pid: %d\n", load[thread].load_pid);
  pfm_load_context(pfm_fd[thread], &load[thread]);
//  fprintf(stderr, "total cycles %"PRIu64"\n", pd[thread][0].reg_value);
//  if (pfm_attach(pfm_fd[thread], 0, (pid_t)syscall(SYS_gettid)) != 0)
//    fprintf(stderr, "pfm_attach failed\n");
  pfm_start(pfm_fd[thread], NULL);
#else
memset(&attr, 0, sizeof(attr));
 pfm_get_perf_event_encoding("PERF_COUNT_HW_CPU_CYCLES",PFM_PLM0|PFM_PLM3, &attr, NULL, NULL);
 if (ret != PFM_SUCCESS)
  fprintf(stderr,"cannot find encoding: %s", pfm_strerror(ret));
 attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING;
 attr.disabled = 1;
 pfm_fd[thread] = perf_event_open(&attr, (pid_t)syscall(__NR_gettid), -1, -1, 0);
// if (pfm_fd[thread] < 0)
//  err(1, "cannot create event");
  /*start counting */
 ret = ioctl(pfm_fd[thread], PERF_EVENT_IOC_ENABLE, 0);
 if (ret)
  fprintf(stderr,"ioctl(enable) failed");
#endif
}

void PFM_output()
{
#ifndef PFM4
  pfm_read_pmds(pfm_fd[0], pd[0], 1);
  fprintf(stderr, "%"PRIu64" %"PRIu64" %"PRIu64" %d %"PRIu64"\n", pd[0][0].reg_value, totalDTTCycle, totalWaitingCycle, totalDTTs, DTT_delay);
#else
  uint64_t values[3];
  read(pfm_fd[0], values, sizeof(values));
//  fprintf(stderr, "%"PRIu64" %"PRIu64" %"PRIu64" %d %"PRIu64"\n", values[0], totalDTTCycle, totalWaitingCycle, totalDTTs, DTT_delay);
  fprintf(stderr, "%llu %llu %llu %d %llu\n", values[0], totalDTTCycle, totalWaitingCycle, totalDTTs, DTT_delay);
#endif
  fprintf(stderr, "tstores: %d\n",tstores);
  #ifdef PFM4
  ioctl(pfm_fd[0], PERF_EVENT_IOC_DISABLE, 0);
  close(pfm_fd[0]);
  pfm_terminate();
  #endif
}
/*
#ifdef MFENCE
void dtt_end_skip(dtt_state *entry)
#else
void dtt_end_skip(volatile dtt_state *entry)
#endif
{
  pfm_read_pmds(pfm_fd, pd, 1);
  PFM_end = pd[0].reg_value;
  #ifdef DTT_THRESHOLD
  if(entry->state == 1)
  entry->stall_cycles=(PFM_end-PFM_start);
  else if(entry->counter >= DTT_THRESHOLD)
  {
   if((PFM_end-PFM_start) > entry->stall_cycles)
   {
    entry->counter = 0;
   }
  }
  #endif
}
*/
#endif
