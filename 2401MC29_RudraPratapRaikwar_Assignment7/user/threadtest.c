#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define THREADS 3
#define ITERATIONS 100000
#define STACK_SIZE 4096

volatile int shared_counter;
char thread_stacks[THREADS][STACK_SIZE];

static void
increment(void *arg)
{
  (void)arg;
  for (int i = 0; i < ITERATIONS; i++) {
    int value = shared_counter;
    if (i % 10000 == 0)
      pause(1);
    for (volatile int delay = 0; delay < 20; delay++)
      ;
    shared_counter = value + 1;
  }
}

int
main(void)
{
  int tids[THREADS];

  shared_counter = 0;
  for (int i = 0; i < THREADS; i++) {
    tids[i] = clone(increment, 0, thread_stacks[i] + STACK_SIZE);
    if (tids[i] < 0) {
      printf("threadtest: clone failed\n");
      exit(1);
    }
  }
  for (int i = 0; i < THREADS; i++) {
    if (join() < 0) {
      printf("threadtest: join failed\n");
      exit(1);
    }
  }
  printf("threadtest: counter=%d expected=%d\n", shared_counter,
         THREADS * ITERATIONS);
  exit(0);
}
