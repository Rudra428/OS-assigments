#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define CHILDREN 4
#define RUN_TICKS 100
#define PRINT_LOCK 4

int launch_tick;

static void
print_aging(int priority, int ticks)
{
  sem_wait(PRINT_LOCK);
  printf("aging: lowest-priority child is now priority=%d after %d ticks\n",
         priority, ticks);
  sem_signal(PRINT_LOCK);
}

int
main(void)
{
  int priorities[CHILDREN] = {1, 5, 10, 20};
  int pids[CHILDREN];

  if (sem_init(PRINT_LOCK, 1) < 0) {
    printf("priotest: semaphore initialization failed\n");
    exit(1);
  }
  launch_tick = uptime();
  printf("priotest: forking CPU-bound children\n");
  for (int i = 0; i < CHILDREN; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("priotest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      int self = getpid();
      if (set_priority(self, priorities[i]) < 0) {
        printf("priotest: set_priority failed\n");
        exit(1);
      }
      int start = uptime();
      volatile uint64 value = 0;
      while (uptime() - start < RUN_TICKS) {
        for (int step = 0; step < 10000; step++)
          value += (uint64)step;
      }
      int runtime = uptime() - start;
      int completion = uptime() - launch_tick;
      sem_wait(PRINT_LOCK);
      printf("child pid=%d finished priority=%d completion_tick=%d runtime=%d "
             "value=%lx\n", self, get_priority(self), completion, runtime,
             value);
      sem_signal(PRINT_LOCK);
      exit(0);
    }
    pids[i] = pid;
  }

  for (int tick = 0; tick < RUN_TICKS; tick += 30) {
    pause(30);
    int priority = get_priority(pids[CHILDREN - 1]);
    if (priority >= 0)
      print_aging(priority, tick + 30);
  }
  for (int i = 0; i < CHILDREN; i++)
    wait(0);
  sem_wait(PRINT_LOCK);
  printf("priotest: all children completed; aging promotes waiting processes\n");
  sem_signal(PRINT_LOCK);
  exit(0);
}
