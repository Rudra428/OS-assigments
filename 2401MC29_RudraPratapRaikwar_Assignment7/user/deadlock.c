#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXP 4
#define MAXR 3

static int
fits(int m, int need[MAXP][MAXR], int p, int work[MAXR])
{
  for (int r = 0; r < m; r++)
    if (need[p][r] > work[r])
      return 0;
  return 1;
}

static void
print_vector(char *name, int m, int values[MAXR])
{
  printf("%s", name);
  for (int r = 0; r < m; r++)
    printf(" %d", values[r]);
  printf("\n");
}

static void
print_detection_state(int n, int m, int allocation[MAXP][MAXR],
                      int request[MAXP][MAXR], int work[MAXR])
{
  print_vector("Available:", m, work);
  for (int p = 0; p < n; p++) {
    printf("  P%d Allocation", p);
    for (int r = 0; r < m; r++)
      printf(" %d", allocation[p][r]);
    printf(" Request");
    for (int r = 0; r < m; r++)
      printf(" %d", request[p][r]);
    printf("\n");
  }
}

static int
detect_deadlock(int n, int m, int allocation[MAXP][MAXR],
                int request[MAXP][MAXR], int available[MAXR],
                int deadlocked[MAXP])
{
  int work[MAXR];
  int finished[MAXP];
  int count = 0;

  for (int r = 0; r < m; r++)
    work[r] = available[r];
  for (int p = 0; p < n; p++) {
    finished[p] = 1;
    for (int r = 0; r < m; r++)
      if (allocation[p][r] != 0)
        finished[p] = 0;
  }

  printf("deadlock detection start\n");
  print_detection_state(n, m, allocation, request, work);
  for (;;) {
    int progress = 0;
    for (int p = 0; p < n; p++) {
      if (!finished[p] && fits(m, request, p, work)) {
        for (int r = 0; r < m; r++)
          work[r] += allocation[p][r];
        finished[p] = 1;
        progress = 1;
        printf("  P%d can finish; ", p);
        printf("\n");
        print_detection_state(n, m, allocation, request, work);
      }
    }
    if (!progress)
      break;
  }

  for (int p = 0; p < n; p++)
    if (!finished[p])
      deadlocked[count++] = p;
  printf("deadlocked processes:");
  if (count == 0)
    printf(" none");
  for (int i = 0; i < count; i++)
    printf(" P%d", deadlocked[i]);
  printf("\n");
  return count;
}

static int
is_safe_state(int n, int m, int allocation[MAXP][MAXR],
              int maximum[MAXP][MAXR], int available[MAXR], int sequence[MAXP])
{
  int work[MAXR];
  int need[MAXP][MAXR];
  int finished[MAXP] = {0};
  int count = 0;

  for (int r = 0; r < m; r++)
    work[r] = available[r];
  for (int p = 0; p < n; p++)
    for (int r = 0; r < m; r++)
      need[p][r] = maximum[p][r] - allocation[p][r];

  printf("Banker state: ");
  print_vector("Available:", m, work);
  for (int p = 0; p < n; p++) {
    printf("  P%d Allocation", p);
    for (int r = 0; r < m; r++)
      printf(" %d", allocation[p][r]);
    printf(" Need");
    for (int r = 0; r < m; r++)
      printf(" %d", need[p][r]);
    printf("\n");
  }

  while (count < n) {
    int found = 0;
    for (int p = 0; p < n; p++) {
      if (!finished[p] && fits(m, need, p, work)) {
        for (int r = 0; r < m; r++)
          work[r] += allocation[p][r];
        finished[p] = 1;
        sequence[count++] = p;
        found = 1;
        printf("  P%d finishes; ", p);
        print_vector("Work:", m, work);
      }
    }
    if (!found)
      return 0;
  }
  return 1;
}

static int
banker_request(int n, int m, int pid, int request[MAXR],
               int allocation[MAXP][MAXR], int maximum[MAXP][MAXR],
               int available[MAXR])
{
  int next_allocation[MAXP][MAXR];
  int next_maximum[MAXP][MAXR];
  int next_available[MAXR];
  int need[MAXR];
  int sequence[MAXP];

  for (int p = 0; p < n; p++) {
    for (int r = 0; r < m; r++) {
      next_allocation[p][r] = allocation[p][r];
      next_maximum[p][r] = maximum[p][r];
    }
  }
  for (int r = 0; r < m; r++) {
    next_available[r] = available[r];
    need[r] = maximum[pid][r] - allocation[pid][r];
    if (request[r] > need[r] || request[r] > available[r])
      return 0;
  }
  for (int r = 0; r < m; r++) {
    next_available[r] -= request[r];
    next_allocation[pid][r] += request[r];
  }
  printf("Hypothetical request by P%d:", pid);
  for (int r = 0; r < m; r++)
    printf(" %d", request[r]);
  printf("\n");
  if (!is_safe_state(n, m, next_allocation, next_maximum, next_available,
                     sequence)) {
    printf("request denied: resulting state is unsafe\n");
    return 0;
  }
  printf("request granted; safe sequence:");
  for (int i = 0; i < n; i++)
    printf(" P%d", sequence[i]);
  printf("\n");
  return 1;
}

static void
pipe_deadlock_demo(void)
{
  int pipe1[2], pipe2[2];
  int a, b;
  char byte;
  int allocation[MAXP][MAXR] = {{1, 0}, {0, 1}};
  int request[MAXP][MAXR] = {{0, 1}, {1, 0}};
  int available[MAXR] = {0, 0};
  int deadlocked[MAXP];

  if (pipe(pipe1) < 0) {
    printf("pipe deadlock demo: pipe failed\n");
    return;
  }
  if (pipe(pipe2) < 0) {
    close(pipe1[0]);
    close(pipe1[1]);
    printf("pipe deadlock demo: pipe failed\n");
    return;
  }
  a = fork();
  if (a == 0) {
    close(pipe1[0]);
    close(pipe2[1]);
    read(pipe2[0], &byte, 1);
    exit(0);
  }
  if (a < 0) {
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    printf("pipe deadlock demo: fork failed\n");
    return;
  }
  b = fork();
  if (b == 0) {
    close(pipe2[0]);
    close(pipe1[1]);
    read(pipe1[0], &byte, 1);
    exit(0);
  }
  close(pipe1[0]);
  close(pipe1[1]);
  close(pipe2[0]);
  close(pipe2[1]);
  if (b < 0) {
    kill(a);
    wait(0);
    printf("pipe deadlock demo: second fork failed\n");
    return;
  }

  sleep(10);
  printf("Pipe wait cycle: P%d holds Pipe1 writer and waits on Pipe2; "
         "P%d holds Pipe2 writer and waits on Pipe1\n", a, b);
  detect_deadlock(2, 2, allocation, request, available, deadlocked);
  kill(a);
  kill(b);
  wait(0);
  wait(0);
  printf("pipe deadlock demo: blocked processes released\n");
}

int
main(void)
{
  int allocation[MAXP][MAXR] = {{1, 0}, {0, 1}};
  int request[MAXP][MAXR] = {{0, 1}, {1, 0}};
  int available[MAXR] = {0, 0};
  int deadlocked[MAXP];

  printf("Case 1: circular wait\n");
  detect_deadlock(2, 2, allocation, request, available, deadlocked);

  printf("Case 2: recoverable state\n");
  allocation[0][0] = 1;
  allocation[0][1] = 0;
  allocation[1][0] = 0;
  allocation[1][1] = 1;
  request[0][0] = 0;
  request[0][1] = 1;
  request[1][0] = 0;
  request[1][1] = 0;
  available[0] = 1;
  available[1] = 0;
  detect_deadlock(2, 2, allocation, request, available, deadlocked);

  printf("Case 3: safe state followed by unsafe request\n");
  allocation[0][0] = 0;
  allocation[0][1] = 0;
  allocation[1][0] = 1;
  allocation[1][1] = 0;
  request[0][0] = 2;
  request[0][1] = 0;
  request[1][0] = 1;
  request[1][1] = 1;
  available[0] = 1;
  available[1] = 1;
  detect_deadlock(2, 2, allocation, request, available, deadlocked);
  int maximum[MAXP][MAXR] = {{2, 0}, {2, 1}};
  int safe_sequence[MAXP];
  if (is_safe_state(2, 2, allocation, maximum, available, safe_sequence)) {
    printf("initial state safe; sequence:");
    for (int i = 0; i < 2; i++)
      printf(" P%d", safe_sequence[i]);
    printf("\n");
  }
  int unsafe_request[MAXR] = {1, 0};
  banker_request(2, 2, 0, unsafe_request, allocation, maximum, available);

  pipe_deadlock_demo();
  exit(0);
}
