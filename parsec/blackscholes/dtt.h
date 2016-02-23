#include <inttypes.h>
#include <pthread.h>
#include <sched.h>

#ifdef PFM

#ifndef PFM4
#include <perfmon/perfmon.h>
#else
#include <perfmon/pfmlib_perf_event.h>
#endif
#include <perfmon/pfmlib.h>
#endif

typedef union {
    double d;
    char c[8];
    int64_t i;
} dtt_value;

typedef struct DTT_EVENT dtt_event;

typedef struct {
  volatile char state;	// 0 for invalid, 1 for valid, 2 for running, 3 for spawning
//  int state;	// 0 for invalid, 1 for valid, 2 for running, 3 for spawning
//  short int counter;
  volatile char cancel;
  pthread_t thread;
#ifdef DTT_THRESHOLD
  int counter;
  int called;
  int stalled;
#else
  dtt_value *target;
#endif
  dtt_value return_value;
#ifdef PFM
  long long int stall_cycles;
#endif
//  volatile int pending;
} dtt_state;


struct DTT_EVENT {
  volatile int valid;
  pthread_t thread;
  dtt_state *state;
  dtt_value *target;
//  dtt_event *next;
  void (*func)(void *x);
  void *args;
};

void tstore(dtt_value *target, dtt_value new_value, int size, dtt_state *entry, void *args, void (*func)(void *x));
dtt_state *dtt_state_create();
//dtt_event *dtt_event_create(dtt_state *state, dtt_value *target);
dtt_event *dtt_event_create(dtt_state *state, dtt_value *target, void *args, void (*func)(void *x));
#ifdef MFENCE
dtt_event *dtt_return(dtt_state *state, dtt_value return_value, dtt_event *event);
#else
dtt_event *dtt_return(volatile dtt_state *state, dtt_value return_value, dtt_event *event);
#endif
dtt_event *dtt_cancel(dtt_state *state);
//#ifdef DTT_POLLING
//#endif
#ifdef MFENCE
void dtt_barrier(dtt_state *entry);
#else
void dtt_barrier(volatile dtt_state *entry);
#endif
#ifdef MFENCE
void dtt_XIP(dtt_state *entry);
#else
void dtt_XIP(volatile dtt_state *entry);
#endif
#ifdef DTT_POLLING
void *dtt(void *x);
#else
void *dtt(void *x);
#endif

#ifdef MFENCE
#define _update_var(_v, _x) { _v = (_x); asm("mfence"); }  
#endif
#ifdef MFENCE
void dtt_end_skip(dtt_state *entry);
#else
void dtt_end_skip(volatile dtt_state *entry);
#endif

#ifdef PFM
void PFM_init(int thread);
void PFM_output();
#endif


#ifdef TIME
//double totalTime;
int invoked, skipped;
#endif
