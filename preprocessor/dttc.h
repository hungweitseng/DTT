#define MAX_LINE_LENGTH 4096

#define DATA_TRIGGER_PRAGMA 0
#define SKIPPABLE_REGION_BEGIN_PRAGMA 1
#define SKIPPABLE_REGION_END_PRAGMA 2
#define SUPPORT_THREAD_FUNCTION_PRAGMA 3
#define MAX_DTT_ENTRIES 128

struct skippable_region_t
{
  char name[MAX_LINE_LENGTH];
};

typedef struct skippable_region_t skippable_region;

struct support_thread_t
{
  char name[MAX_LINE_LENGTH];
  char skippable_region[MAX_LINE_LENGTH];
};

typedef struct support_thread_t support_thread;

struct data_trigger_t
{
  char type[MAX_LINE_LENGTH];
  char name[MAX_LINE_LENGTH];
  char support_thread[MAX_LINE_LENGTH];
  support_thread *state_variable;
  char function[MAX_LINE_LENGTH];
  char filename[MAX_LINE_LENGTH];
};

typedef struct data_trigger_t data_trigger;

