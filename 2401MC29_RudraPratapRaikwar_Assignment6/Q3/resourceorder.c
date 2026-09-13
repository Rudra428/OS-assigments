#include "kernel/types.h"
#include "user/user.h"

// Shared memory structure to track process completion.
// Necessary because xv6 processes have separate address spaces, 
// so standard global variables won't be shared across fork().
struct shared {
  int bad_ready;
  int fixed_done[2];
};

/*
 * fixed_process: Worker function for the deadlock-free scenario.
 * DEADLOCK PREVENTION STRATEGY: Resource Ordering.
 * By ensuring all processes acquire locks in the EXACT SAME ORDER
 * (Lock1 first, then Lock2), a circular wait condition is mathematically impossible.
 */
static void
fixed_process(struct shared *s, int id, int lock1, int lock2)
{
  printf("[fixed] Process %d requesting Lock1\n", id);
  sem_wait(lock1); // First lock acquired
  printf("[fixed] Process %d acquired Lock1\n", id);
  
  // Pause forces a context switch, giving the other process a chance to run.
  // In the bad version, this causes deadlock. Here, it is perfectly safe.
  pause(1); 
  
  printf("[fixed] Process %d requesting Lock2\n", id);
  sem_wait(lock2); // Second lock acquired
  printf("[fixed] Process %d acquired Lock2\n", id);
  
  // Critical Section
  printf("[fixed] Process %d completed work\n", id);
  
  // Release locks (order of release doesn't strictly matter for deadlock prevention)
  sem_post(lock2);
  sem_post(lock1);
  printf("[fixed] Process %d released Lock2 and Lock1\n", id);
  
  // Mark this process as successfully completed in shared memory
  s->fixed_done[id] = 1;
  exit(0);
}

/*
 * run_bad: Demonstrates a classic Deadlock (Circular Wait).
 * Process A holds Lock 1 and waits for Lock 2.
 * Process B holds Lock 2 and waits for Lock 1.
 * Both sleep indefinitely waiting for the other to release the resource.
 */
static int
run_bad(void)
{
  struct shared *s = (struct shared *)shm_get();
  int lock1 = sem_create(1); // Binary semaphore 1
  int lock2 = sem_create(1); // Binary semaphore 2
  
  if (s == 0 || lock1 < 0 || lock2 < 0) {
    printf("[bad] setup failed\n");
    exit(1);
  }
  
  printf("[bad] Starting intentional circular wait; stop xv6 with Ctrl-A X or timeout\n");
  
  // --- Create Process A ---
  int pid = fork();
  if (pid < 0) exit(1);
  
  if (pid == 0) {
    sem_wait(lock1); // Process A takes Lock 1
    printf("[bad] Process A acquired Lock1\n");
    
    // Pause guarantees Process B has time to start and grab Lock 2
    pause(3); 
    
    printf("[bad] Process A waiting for Lock2\n");
    sem_wait(lock2); // Process A gets stuck here (Deadlock)
    exit(0);
  }
  
  // Stagger the start of Process B slightly
  pause(1); 
  
  // --- Create Process B ---
  pid = fork();
  if (pid < 0) exit(1);
  
  if (pid == 0) {
    sem_wait(lock2); // Process B takes Lock 2
    printf("[bad] Process B acquired Lock2\n");
    
    // Pause to ensure Process A is fully entrenched
    pause(3); 
    
    printf("[bad] Process B waiting for Lock1\n");
    sem_wait(lock1); // Process B gets stuck here (Deadlock)
    exit(0);
  }
  
  // The parent process waits for children to finish.
  // Because of the deadlock, wait(0) will never return.
  wait(0);
  wait(0);
  
  // Cleanup (this code is theoretically unreachable in this scenario)
  sem_destroy(lock1);
  sem_destroy(lock2);
  return 0;
}

/*
 * run_fixed: Demonstrates safe resource allocation.
 * Spawns two processes that both adhere to strict resource ordering.
 */
static int
run_fixed(void)
{
  struct shared *s = (struct shared *)shm_get();
  int lock1 = sem_create(1);
  int lock2 = sem_create(1);
  
  if (s == 0 || lock1 < 0 || lock2 < 0) {
    printf("[fixed] setup failed\n");
    exit(1);
  }
  
  // Initialize shared completion flags
  s->fixed_done[0] = 0;
  s->fixed_done[1] = 0;
  
  // Fork two child processes
  for (int i = 0; i < 2; i++) {
    int pid = fork();
    if (pid < 0) exit(1);
    
    if (pid == 0) {
      fixed_process(s, i, lock1, lock2); // Children jump into the safe worker function
    }
  }
  
  // Parent waits for both safe processes to finish
  wait(0);
  wait(0);
  
  // Verify both processes set their completion flags in shared memory
  int ok = s->fixed_done[0] && s->fixed_done[1];
  
  // Cleanup resources
  sem_destroy(lock1);
  sem_destroy(lock2);
  
  printf("[fixed] Both processes completed: %s\n", ok ? "YES" : "NO");
  return ok ? 0 : 1;
}

int
main(int argc, char **argv)
{
  // Simple command-line argument parsing to trigger the scenarios
  if (argc > 1 && argv[1][0] == 'b')
    exit(run_bad());
    
  if (argc > 1 && argv[1][0] == 'f')
    exit(run_fixed());
    
  // Default fall-through if arguments are missing or incorrect
  printf("usage: resourceorder bad | fixed\n");
  exit(1);
}