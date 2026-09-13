#include "kernel/types.h"
#include "user/user.h"

#define PROCESSES 5
#define RESOURCES 3
#define CYCLES 3

struct shared {
  int completed[PROCESSES];
};

// Resource ordering: first index is always < second index to prevent circular wait
static int needs[PROCESSES][2] = {
  {0, 1}, {0, 2}, {1, 2}, {0, 1}, {1, 2}};
static char *resource_names[RESOURCES] = {"Printer", "Scanner", "Disk"};

static void
worker(struct shared *s, int sems[RESOURCES], int id)
{
  int first = needs[id][0];
  int second = needs[id][1];
  for (int cycle = 1; cycle <= CYCLES; cycle++) {
    // Acquire resources in strict ascending order (first then second)
    printf("P%d cycle %d: requesting %s\n", id, cycle,
           resource_names[first]);
    sem_wait(sems[first]);
    printf("P%d cycle %d: granted %s\n", id, cycle,
           resource_names[first]);

    printf("P%d cycle %d: requesting %s\n", id, cycle,
           resource_names[second]);
    sem_wait(sems[second]);
    printf("P%d cycle %d: granted %s; starting work\n", id, cycle,
           resource_names[second]);

    // Critical section
    pause(1);

    // Release both resources
    printf("P%d cycle %d: releasing %s and %s\n", id, cycle,
           resource_names[second], resource_names[first]);
    sem_post(sems[second]);
    sem_post(sems[first]);
  }
  s->completed[id] = CYCLES;
  printf("P%d completed %d cycles\n", id, CYCLES);
  exit(0);
}

int
main(void)
{
  struct shared *s = (struct shared *)shm_get();
  int sems[RESOURCES];
  int capacities[RESOURCES] = {2, 1, 2}; // Instance count per resource type
  if (s == 0) {
    printf("syncdeadlock: shared memory setup failed\n");
    exit(1);
  }
  printf("syncdeadlock: global order Printer < Scanner < Disk\n");
  printf("syncdeadlock: capacities Printer=2 Scanner=1 Disk=2\n");
  for (int i = 0; i < PROCESSES; i++)
    s->completed[i] = 0;

  // Initialize counting semaphores
  for (int i = 0; i < RESOURCES; i++) {
    sems[i] = sem_create(capacities[i]);
    if (sems[i] < 0) {
      printf("syncdeadlock: semaphore setup failed\n");
      exit(1);
    }
  }

  // Spawn worker processes
  for (int i = 0; i < PROCESSES; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("syncdeadlock: fork failed\n");
      exit(1);
    }
    if (pid == 0)
      worker(s, sems, i);
  }

  // Wait for all workers to finish
  for (int i = 0; i < PROCESSES; i++)
    wait(0);

  // Verify all workers completed their cycles without deadlock
  int ok = 1;
  for (int i = 0; i < PROCESSES; i++)
    if (s->completed[i] != CYCLES)
      ok = 0;
  for (int i = 0; i < RESOURCES; i++)
    sem_destroy(sems[i]);
  printf("syncdeadlock: all processes completed=%s\n", ok ? "YES" : "NO");
  exit(ok ? 0 : 1);
}