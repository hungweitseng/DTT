#include <stdio.h>
#include <stdlib.h>
#include "dtt.h"
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#ifdef DTT_THRESHOLD
int dtt_threshold = DTT_THRESHOLD;
#endif
inline void atomic_increment(volatile int *pw);
inline void atomic_decrement(volatile int *pw);

#ifdef TIME
#include <sys/time.h>
struct timeval timestamp;
#endif


#ifndef CPU_MAX
#define CPU_MAX 16
#endif
int number_of_threads;

unsigned int EQ_head; // The next to commit
unsigned int EQ_tail;
int dtt_tid;

#ifndef dtt_lock
inline int atomic_compare_and_exchange(int requiredOldValue, volatile int* _ptr, int newValue, int sizeOfValue);
#define dtt_lock(var) {while(atomic_compare_and_exchange(0,&var,1,sizeof(var)) !=0 );}
#define dtt_try_lock(var) atomic_compare_and_exchange(0,&var,1,sizeof(var))
#define dtt_unlock(var) {atomic_compare_and_exchange(1,&var,0,sizeof(var));}
#endif

dtt_event __attribute__((aligned(64))) EQ[EQ_MAX];

int dtt_init()
{
  pthread_t dtt_thread;
  pthread_create(&dtt_thread, NULL, dtt, NULL);
  pthread_detach(dtt_thread);
  return 0;
}

inline void dtt_event_create(dtt_state *state, void *args, void* (*func)(void *x))
{
  dtt_event *dtt_arg = NULL;
  unsigned int new_EQ_tail;
  int i,found=0,stop,now;
  // New event in the state variable
  atomic_increment(&state->pending);
  // Invalidate the state variable
  state->state = 0;
  // The location of new queue tail
   new_EQ_tail = (EQ_tail+1) % (EQ_MAX);
   i=new_EQ_tail;

   dtt_arg = NULL;
   do
   {
       now = i;
       if(EQ[now].valid == 0)
       {
         // Try to grab the lock
         if(dtt_try_lock( EQ[now].lock ))
          continue;
          // Successfully grab an empty EQ entry, enqueue the event
         if(EQ[now].valid == 0)
         {
           dtt_arg=&EQ[now];
           dtt_arg->valid = 1;
           dtt_arg->state = state;
           dtt_arg->func = func;
           dtt_arg->args = args;
           dtt_unlock( EQ[now].lock );
//           return;
         }
         // No free entry, another run
         else{
           dtt_unlock( EQ[now].lock );
         }
       }
   }
   while(dtt_arg == NULL);
#ifdef MFENCE
  _update_var(EQ_tail, new_EQ_tail);
#else
  EQ_tail = new_EQ_tail;
#endif
  return;
}

dtt_state *dtt_state_create()
{
  dtt_state *temp = (dtt_state *)calloc(1,sizeof(dtt_state));
  temp->cancel = 0;
  temp->pending = 0;
  return temp;
}

inline void tstore(int size, dtt_state *entry, void *args, void* (*func)(void *x), void *new_value, void *old_value)
{
  dtt_event *dtt_instance = NULL;
  int i = 0;
//  va_list arguments;
  double old, new;
    if(entry->cancel)
    {
      entry->state = 0;
      return;
    }
//     old=*(double *)old_value;
//     new=*(double *)new_value;
//      fprintf(stderr, "old: %lf new: %lf\n",old, new);
    if(size == 0 || !(!memoryCompare(new_value, old_value, size)))
//    if(size == 0 || memoryCompare(new_value, old_value, size))
    {
      dtt_event_create(entry, new_value, func);
//      fprintf(stderr, "Trigger: old: %.15lf new: %.15lf\n",old, new);
    }
    return;
}

inline void dtt_return(dtt_state *state, dtt_event *event)
{
  event->valid = 0;
  if(state->pending == 0)
    state->state = 1;
  return;
}

void dtt_invalidate(dtt_state *state)
{
  state->state = 0;
}

void dtt_validate(dtt_state *state)
{
  state->state = 1;
  state->cancel = 0;
}

void *dtt(void *x)
{
  int *parameter = (int *)x;
  dtt_event *current = NULL;
  dtt_event *old = NULL;
  int i=0;
//  int thisThread = *parameter;
  int now, stop; // Loop control variables
  int sum = 0;

  dtt_tid = (pid_t)syscall(__NR_gettid);
  while(1)
  {
        current = NULL;
        i = EQ_head;
        stop = i+EQ_MAX;
        for(; i!= stop ; i++)
        {
            // Examine EQ[now]
            now = i % (EQ_MAX);
            // EQ[now] contains a pending event, execute it.
            if(EQ[now].valid == 1)
            {
                // Grab lock of this entry
                if(dtt_try_lock( EQ[now].lock))
                {
                 continue;
                }
                current = &EQ[now];
                // The EQ[now] is grabbed by others... Give up. Should not happen anyway
                if(current->valid != 1)
                {
                    dtt_unlock( EQ[now].lock );
                    current = NULL;
                    fprintf(stderr, "DTT runtime error: Lock grabbed\n");
                    continue;
                }
                // Mark this event as running
                current->valid = 2;
//                current->state->state = 2;

                current->func(current->args);
                current->valid = 0;
                atomic_decrement(&current->state->pending);
                if(current->state->pending == 0)
                  current->state->state = 1;
                // Release the lock
                dtt_unlock( current->lock );

            }
        }
//        usleep(0);
  }
  pthread_exit(NULL);
  return NULL;
}

#ifdef MFENCE
void dtt_barrier(dtt_state *entry)
#else
void dtt_barrier(volatile dtt_state *entry)
#endif
{
 int i;
#ifdef PFM4
 uint64_t values[3];
#endif

#ifndef SINGLE_THREAD
  if(entry && (entry->pending != 0 || entry->state == 2) && entry->cancel == 0)
  {
#ifdef DTT_XIP
    dtt_XIP();
#endif
//    counters[2]++;
#ifdef DTT_THRESHOLD
    entry->stalled++;
#endif
#ifdef PFM
#ifndef PFM4
  pfm_read_pmds(pfm_fd[0], pd[0], 1);
  PFM_start = pd[0][0].reg_value;
#else
//  read(pfm_fd[0], &PFM_start, sizeof(PFM_start));
  read(pfm_fd[0], values, sizeof(values));
  PFM_start = values[0];
#endif
#endif
#ifdef DTT_PENDING_COUNT
  while(entry->pending != 0)
#else
  while(entry->state == 2)
#endif
  {
#ifdef DTT_XIP
    dtt_XIP();
#endif
//    counters[2]++;
    #ifdef DTT_THRESHOLD
//      entry->counter++;
    #endif
    #ifndef DTT_PENDING_COUNT
        for(i = 0; i < EQ_MAX ; i++){
            if(EQ[i].valid >= 1 && EQ[i].state == entry)
            {
              entry->state = 2;
                break;
            }
        }
        if(i == EQ_MAX)
        {
          entry->state = 1;
//          break;
        }
     #endif
  }

//    in_barrier = 0;    
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
  if((entry->called & 0x400) == 0)
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
entry->cancel = 0;
}

// Atomic operations
inline int atomic_compare_and_exchange(int requiredOldValue, volatile int* _ptr, int newValue, int sizeOfValue)
{
	  int old;
	  
  	  __asm volatile
  	       (
		"mov %3, %%eax;\n\t"
  	       	"lock\n\t"
  		"cmpxchg %4, %0\n\t"
  		"mov %%eax, %1\n\t"
		:
  		"=m" ( *_ptr ), "=r" ( old  ): // outputs (%0 %1)
  		"m" ( *_ptr ), "r" ( requiredOldValue), "r" ( newValue ): // inputs (%2, %3, %4)
  		"memory", "eax", "cc" // clobbers
  		);

  	  return old;
}

inline void atomic_increment(volatile int *pw)
{
	  __asm (
	       "lock\n\t"
	       "incl %0":
	       "=m"(*pw): // output (%0)
	       "m"(*pw): // input (%1)
	       "cc" // clobbers
	       );
}
inline void atomic_decrement(volatile int *pw)
{
	  __asm (
	       "lock\n\t"
	       "decl %0":
	       "=m"(*pw): // output (%0)
	       "m"(*pw): // input (%1)
	       "cc" // clobbers
	       );
}

inline int memoryCompare(const void* lhs, const void* rhs, size_t n) 
{
    for(; n >= sizeof(int); n -= sizeof(int)) {
        if( (int *)lhs < (int *)rhs) {
            return -1;
        } else if( (int *)lhs > (int *)rhs) {
            return 1;
        }
        lhs = (char *)lhs+sizeof(int);
        rhs = (char *)rhs+sizeof(int);
    }

    for(; n >= sizeof(char); n -= sizeof(char)) {
        if( (char *) lhs < (char *) rhs) {
            return -1;
        } else if( (char *) lhs > (char *) rhs) {
            return 1;
        }
        lhs = (char *)lhs+sizeof(char);
        rhs = (char *)rhs+sizeof(char);
    }

    return 0;
}

