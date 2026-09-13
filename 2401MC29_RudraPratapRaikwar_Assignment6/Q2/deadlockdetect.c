#include "kernel/types.h"
#include "user/user.h"

// Define simulation constraints: 4 tasks (P0-P3) and 3 single-instance resources (R0-R2)
#define NUM_TASKS 4
#define NUM_RSRC 3

/*
 * display_matrix: Helper to format and output state matrices (Allocation / Request).
 */
static void
display_matrix(char *title, int mat[NUM_TASKS][NUM_RSRC])
{
  printf("%s:\n", title);
  for (int t = 0; t < NUM_TASKS; t++)
    printf("  Task %d: %d %d %d\n", t, mat[t][0], mat[t][1], mat[t][2]);
}

/*
 * construct_wait_graph: Converts Allocation and Request state into a Wait-For Graph (WFG).
 * In single-unit resource systems:
 * If Task A requests resource R, and Task B holds resource R, 
 * an edge A -> B is added (Task A is waiting for Task B to release R).
 */
static void
construct_wait_graph(int alloc_mat[NUM_TASKS][NUM_RSRC],
                     int req_mat[NUM_TASKS][NUM_RSRC],
                     int adj_matrix[NUM_TASKS][NUM_TASKS])
{
  // Clear adjacency matrix
  for (int i = 0; i < NUM_TASKS; i++)
    for (int j = 0; j < NUM_TASKS; j++)
      adj_matrix[i][j] = 0;

  // Scan every process and resource for outstanding requests
  for (int req_proc = 0; req_proc < NUM_TASKS; req_proc++) {
    for (int r = 0; r < NUM_RSRC; r++) {
      if (req_mat[req_proc][r] != 0) {
        // Find which process currently holds this requested resource
        for (int own_proc = 0; own_proc < NUM_TASKS; own_proc++) {
          if (alloc_mat[own_proc][r] != 0) {
            adj_matrix[req_proc][own_proc] = 1; // Directed dependency edge: req_proc -> own_proc
          }
        }
      }
    }
  }
}

/*
 * display_edges: Prints all directed dependencies present in the Wait-For Graph.
 */
static void
display_edges(int adj_matrix[NUM_TASKS][NUM_TASKS])
{
  printf("Wait-for Graph Edges:\n");
  for (int i = 0; i < NUM_TASKS; i++)
    for (int j = 0; j < NUM_TASKS; j++)
      if (adj_matrix[i][j])
        printf("  Task %d -> Task %d\n", i, j);
}

/*
 * detect_cycle_dfs: Uses 3-color Depth First Search to detect cycles in a directed graph.
 * Node states:
 *   0: Unvisited (White)
 *   1: Currently visiting / on recursion call stack (Gray)
 *   2: Fully explored and backtracking (Black)
 * A cycle occurs when an active edge points to a node with state == 1 (back edge).
 */
static int
detect_cycle_dfs(int node, int adj_matrix[NUM_TASKS][NUM_TASKS], int visited[NUM_TASKS],
                 int trace[NUM_TASKS], int level, int deadlock_loop[NUM_TASKS], int *loop_size)
{
  visited[node] = 1;      // Mark node as currently on recursion stack
  trace[level++] = node;  // Record path for cycle reconstruction
  
  for (int next_node = 0; next_node < NUM_TASKS; next_node++) {
    if (adj_matrix[node][next_node] == 0)
      continue;
      
    // Recurse on unvisited neighbors
    if (visited[next_node] == 0 &&
        detect_cycle_dfs(next_node, adj_matrix, visited, trace, level, deadlock_loop, loop_size)) {
      return 1;
    }
    
    // Found back edge to an ancestor currently on the stack -> Cycle detected
    if (visited[next_node] == 1) {
      // Find where the cycle begins along the path trace
      int start_idx = 0;
      while (trace[start_idx] != next_node)
        start_idx++;
        
      // Extract the cycle loop vertices
      *loop_size = 0;
      for (int k = start_idx; k < level; k++)
        deadlock_loop[(*loop_size)++] = trace[k];
      deadlock_loop[(*loop_size)++] = next_node; // Close loop for display
      return 1;
    }
  }
  
  visited[node] = 2; // Mark fully processed
  return 0;
}

/*
 * evaluate_scenario: Orchestrates graph construction, edge printing, and cycle check.
 */
static void
evaluate_scenario(char *desc, int alloc_mat[NUM_TASKS][NUM_RSRC],
                  int req_mat[NUM_TASKS][NUM_RSRC])
{
  int adj_matrix[NUM_TASKS][NUM_TASKS];
  int visited[NUM_TASKS] = {0};
  int trace[NUM_TASKS];
  int deadlock_loop[NUM_TASKS + 1];
  int loop_size = 0;
  int has_deadlock = 0;

  printf("\n--- %s ---\n", desc);
  display_matrix("Current Allocation", alloc_mat);
  display_matrix("Pending Requests", req_mat);
  
  construct_wait_graph(alloc_mat, req_mat, adj_matrix);
  display_edges(adj_matrix);
  
  // Run DFS from each unvisited component
  for (int i = 0; i < NUM_TASKS && has_deadlock == 0; i++) {
    if (visited[i] == 0) {
      has_deadlock = detect_cycle_dfs(i, adj_matrix, visited, trace, 0, deadlock_loop, &loop_size);
    }
  }
    
  // If graph is acyclic, no circular wait exists
  if (has_deadlock == 0) {
    printf("Analysis: SYSTEM SAFE (No circular wait detected)\n");
    return;
  }
  
  // Report circular dependency path
  printf("Analysis: DEADLOCK DETECTED\nCircular Dependency: ");
  for (int i = 0; i < loop_size; i++)
    printf("Task %d%s", deadlock_loop[i], (i == loop_size - 1) ? "\n" : " -> ");
}

int
main(void)
{
  // Test Case 1: Linear chain of dependencies (Task 3 -> Task 0 -> Task 1 -> Task 2)
  int safe_alloc[NUM_TASKS][NUM_RSRC] = {
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
  int safe_req[NUM_TASKS][NUM_RSRC] = {
    {0, 1, 0}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}};
    
  // Test Case 2: Closed loop dependency (Task 0 -> Task 1 -> Task 2 -> Task 0)
  int deadlocked_alloc[NUM_TASKS][NUM_RSRC] = {
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};
  int deadlocked_req[NUM_TASKS][NUM_RSRC] = {
    {0, 1, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, 0}};

  printf(">> Deadlock Detection via Wait-For Graph <<\n");
  
  evaluate_scenario("Test Case 1: Acyclic Wait-For Graph", safe_alloc, safe_req);
  evaluate_scenario("Test Case 2: 3-Process Circular Wait", deadlocked_alloc, deadlocked_req);
  
  exit(0); // Standard xv6 process exit
}