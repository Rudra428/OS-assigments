#include "kernel/types.h"
#include "user/user.h"

// System parameters: 5 simulated processes and 3 distinct resource types (e.g., A, B, C)
#define NUM_PROCS 5
#define NUM_RES 3

// Allocation matrix: instances of each resource currently held by each process
static int alloc_table[NUM_PROCS][NUM_RES] = {
  {0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};

// Max matrix: maximum demand declared by each process over its execution lifetime
static int max_demand[NUM_PROCS][NUM_RES] = {
  {7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};

// Available vector: unallocated units of each resource type currently in the pool
static int current_avail[NUM_RES] = {3, 3, 2};

/*
 * show_matrix: Utility routine to display resource matrices in a tabular format.
 */
static void
show_matrix(char *label, int mat[NUM_PROCS][NUM_RES])
{
  printf("%s:\n", label);
  for (int p = 0; p < NUM_PROCS; p++) {
    printf("  Proc %d: %d %d %d\n", p, mat[p][0], mat[p][1], mat[p][2]);
  }
}

/*
 * build_need_matrix: Calculates the remaining resource requirements for each task.
 * Mathematical relationship: Need[i][j] = Max[i][j] - Allocation[i][j]
 */
static void
build_need_matrix(int need_mat[NUM_PROCS][NUM_RES])
{
  for (int p = 0; p < NUM_PROCS; p++) {
    for (int r = 0; r < NUM_RES; r++) {
      need_mat[p][r] = max_demand[p][r] - alloc_table[p][r];
    }
  }
}

/*
 * can_execute: Checks if current available/working resources can satisfy 
 * the remaining requirements of a specific process.
 * Returns 1 if for all resource types r: Need[p_id][r] <= Work[r], else 0.
 */
static int
can_execute(int p_id, int running_work[NUM_RES], int need_mat[NUM_PROCS][NUM_RES])
{
  for (int r = 0; r < NUM_RES; r++) {
    if (need_mat[p_id][r] > running_work[r]) {
      return 0; // At least one resource requirement exceeds current working pool
    }
  }
  return 1;
}

/*
 * check_safe_state: Implements Dijkstra's Safety Algorithm.
 * Simulates process execution by assuming processes that have their needs met 
 * will complete and return all currently allocated resources back to the pool.
 * Returns 1 if a full safe sequence exists, 0 otherwise.
 */
static int
check_safe_state(int exec_order[NUM_PROCS])
{
  int need_mat[NUM_PROCS][NUM_RES];
  int running_work[NUM_RES]; // Work vector representing dynamically available resources
  int done[NUM_PROCS];       // Finish vector tracking simulated process completions
  int completed = 0;

  build_need_matrix(need_mat);

  // Step 1: Initialize Work = Available, Finish[i] = 0 for all i
  for (int r = 0; r < NUM_RES; r++)
    running_work[r] = current_avail[r];

  for (int p = 0; p < NUM_PROCS; p++)
    done[p] = 0;

  // Step 2 & 3: Iteratively find an unfinished process whose need can be satisfied
  while (completed < NUM_PROCS) {
    int found_any = 0;
    for (int p = 0; p < NUM_PROCS; p++) {
      if (done[p] == 0 && can_execute(p, running_work, need_mat)) {
        // Assume process completes: reclaim its allocated resources
        for (int r = 0; r < NUM_RES; r++) {
          running_work[r] += alloc_table[p][r];
        }
        done[p] = 1;
        exec_order[completed++] = p; // Record process in the safe sequence
        found_any = 1;
      }
    }
    // If a full iteration across all processes yielded no progress, system is unsafe
    if (found_any == 0)
      return 0; 
  }
  return 1; // All processes finished successfully without deadlock
}

/*
 * process_request: Implements the Resource-Request Algorithm.
 * Validates the request, performs a trial allocation, runs the safety algorithm,
 * and either commits the transaction or rolls it back to prevent unsafe states.
 */
static int
process_request(int p_id, int req_vec[NUM_RES])
{
  int need_mat[NUM_PROCS][NUM_RES];
  int exec_order[NUM_PROCS];

  build_need_matrix(need_mat);
  printf("Proc %d is requesting: %d %d %d\n", p_id, req_vec[0], req_vec[1], req_vec[2]);

  // Validation Phase: Check limits against declared Max Need and current Available
  for (int r = 0; r < NUM_RES; r++) {
    if (req_vec[r] > need_mat[p_id][r]) {
      printf("Error: Request is larger than max need for Proc %d\n", p_id);
      return 0;
    }
    if (req_vec[r] > current_avail[r]) {
      printf("Error: Not enough resources currently available\n");
      return 0;
    }
  }

  // Trial Allocation Phase: Pretend to grant resources to p_id
  for (int r = 0; r < NUM_RES; r++) {
    current_avail[r] -= req_vec[r];
    alloc_table[p_id][r] += req_vec[r];
  }

  // Safety Evaluation: Verify whether the trial state remains safe
  if (check_safe_state(exec_order)) {
    // Commit: State is verified safe, keep the allocation
    printf("Success: Allocation is safe\n");
    printf("New Execution Sequence: ");
    for (int i = 0; i < NUM_PROCS; i++) {
      printf("P%d%s", exec_order[i], (i == NUM_PROCS - 1) ? "\n" : " -> ");
    }
    return 1;
  }

  // Rollback Phase: Undo trial allocation if state turns out unsafe
  for (int r = 0; r < NUM_RES; r++) {
    current_avail[r] += req_vec[r];
    alloc_table[p_id][r] -= req_vec[r];
  }
  printf("Rejected: System would enter unsafe state\n");
  printf("Reverting temporary allocation...\n");
  return 0;
}

int
main(void)
{
  int need_mat[NUM_PROCS][NUM_RES];
  int exec_order[NUM_PROCS];
  
  // Sample test requests
  int test_req1[NUM_RES] = {1, 0, 2}; // Valid request by Proc 1 (remains safe)
  int test_req2[NUM_RES] = {2, 3, 0}; // Invalid request by Proc 4 (leads to unsafe state)

  // 1. Initial State Visualization
  printf("--- xv6 Banker's Algorithm ---\n");
  show_matrix("Current Allocation", alloc_table);
  show_matrix("Maximum Demand", max_demand);
  printf("Available Resources: %d %d %d\n", current_avail[0], current_avail[1], current_avail[2]);
  
  build_need_matrix(need_mat);
  show_matrix("Need Matrix", need_mat);

  // 2. Initial Safety Check
  if (check_safe_state(exec_order)) {
    printf("Status: SAFE\nBase Sequence: ");
    for (int i = 0; i < NUM_PROCS; i++) {
      printf("P%d%s", exec_order[i], (i == NUM_PROCS - 1) ? "\n" : " -> ");
    }
  } else {
    printf("Status: UNSAFE DEADLOCK DETECTED\n");
  }

  // 3. Scenario 1: Safe Resource Request
  printf("\n--- Test 1 (Valid) ---\n");
  process_request(1, test_req1);
  printf("Pool Post-Test 1: %d %d %d\n", current_avail[0], current_avail[1], current_avail[2]);

  // 4. Scenario 2: Unsafe Resource Request (Should trigger rollback)
  printf("\n--- Test 2 (Invalid) ---\n");
  process_request(4, test_req2);
  printf("Pool Post-Test 2: %d %d %d\n", current_avail[0], current_avail[1], current_avail[2]);
  
  exit(0); // Terminate user program via xv6 syscall
}