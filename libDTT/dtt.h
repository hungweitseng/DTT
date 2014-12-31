#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <perfmon/pfmlib.h>

#define EQ_MAX 4096

typedef union {
    double d;
    char c[8];
    int64_t i;
} dtt_value;

typedef struct DTT_EVENT dtt_event;

typedef struct {
  volatile char state;	// 0 for invalid, 1 for valid, 2 for running, 3 for spawning
//  int state;	// 0 for invalid, 1 for valid, 2 for running, 3 for spawning, 4 for speculating
//  short int counter;
  pthread_t thread;
//  dtt_value return_value;
  volatile char cancel;
  volatile int pending;
} dtt_state;


struct DTT_EVENT {
  int valid; // 1 for valid/pending, 2 for running, 0 for invalid, 3 for committing // 8 bytes
  dtt_state *state; // 8 bytes
  void* (*func)(void *x); // 8 bytes
  void *args; // 8 bytes
  int lock; //  8 bytes
  dtt_value *target;
  dtt_event *prev;
  dtt_event *next;
};

inline void tstore(int size, dtt_state *entry, void *args, void* (*func)(void *x), void *new_value, void *old_value);

dtt_state *dtt_state_create();

inline void dtt_event_create(dtt_state *state, void *args, void* (*func)(void *x));

inline void dtt_return(dtt_state *state, dtt_event *event);

dtt_event *dtt_cancel(dtt_state *state);
#ifdef MFENCE
void dtt_barrier(dtt_state *entry);
#else
void dtt_barrier(volatile dtt_state *entry);
#endif

void *dtt(void *x);
int dtt_init();

#define _update_var(_v, _x) { _v = (_x); asm("mfence"); }  

void dtt_invalidate(dtt_state *state);
void tstore_invalidate(int size, dtt_state *entry, void *args, void (*func)(void *x), void *old_value, void *new_value);
inline int memoryCompare(const void* lhs, const void* rhs, size_t n);

extern dtt_event __attribute__((aligned(64))) EQ[EQ_MAX];

