#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define COUNTER_THREADS 3
#define ITEMS 20
#define BUFFER_SIZE 5
#define STACK_SIZE 4096

#define COUNTER_MUTEX 0
#define BUFFER_MUTEX 1
#define EMPTY_SLOTS 2
#define FULL_SLOTS 3

volatile int shared_counter;
int buffer[BUFFER_SIZE];
int in;
int out;
char thread_stacks[5][STACK_SIZE];

static void
increment(void *arg)
{
  (void)arg;
  for (int i = 0; i < 100000; i++) {
    sem_wait(COUNTER_MUTEX);
    shared_counter++;
    sem_signal(COUNTER_MUTEX);
  }
}

static void
producer(void *arg)
{
  (void)arg;
  for (int item = 1; item <= ITEMS; item++) {
    sem_wait(EMPTY_SLOTS);
    sem_wait(BUFFER_MUTEX);
    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;
    printf("producer: %d\n", item);
    sem_signal(BUFFER_MUTEX);
    sem_signal(FULL_SLOTS);
  }
}

static void
consumer(void *arg)
{
  (void)arg;
  for (int i = 0; i < ITEMS; i++) {
    sem_wait(FULL_SLOTS);
    sem_wait(BUFFER_MUTEX);
    int item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    printf("consumer: %d\n", item);
    sem_signal(BUFFER_MUTEX);
    sem_signal(EMPTY_SLOTS);
  }
}

int
main(void)
{
  for (int i = 0; i < 4; i++)
    if (sem_init(i, i == COUNTER_MUTEX || i == BUFFER_MUTEX ? 1 :
                    (i == EMPTY_SLOTS ? BUFFER_SIZE : 0)) < 0) {
      printf("synctest: sem_init failed\n");
      exit(1);
    }

  shared_counter = 0;
  for (int i = 0; i < COUNTER_THREADS; i++)
    if (clone(increment, 0, thread_stacks[i] + STACK_SIZE) < 0) {
      printf("synctest: counter clone failed\n");
      exit(1);
    }
  for (int i = 0; i < COUNTER_THREADS; i++)
    if (join() < 0) {
      printf("synctest: counter join failed\n");
      exit(1);
    }
  printf("synctest: counter=%d expected=300000\n", shared_counter);

  in = 0;
  out = 0;
  if (clone(producer, 0, thread_stacks[3] + STACK_SIZE) < 0 ||
      clone(consumer, 0, thread_stacks[4] + STACK_SIZE) < 0) {
    printf("synctest: producer/consumer clone failed\n");
    exit(1);
  }
  if (join() < 0 || join() < 0) {
    printf("synctest: producer/consumer join failed\n");
    exit(1);
  }
  printf("synctest: producer-consumer complete\n");
  exit(0);
}
