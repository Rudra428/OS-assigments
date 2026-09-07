#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_PHIL 5
#define CYCLES 5

// Fork semaphores are indexed 0 to 4
void philosopher(int id) {
    int left_fork = id;
    int right_fork = (id + 1) % NUM_PHIL;

    // Resource Ordering: always pick the lower-indexed fork first
    int first_fork = (left_fork < right_fork) ? left_fork : right_fork;
    int second_fork = (left_fork < right_fork) ? right_fork : left_fork;

    for (int i = 1; i <= CYCLES; i++) {
        // --- THINKING ---
        printf("[%d] Philosopher %d: THINKING (cycle %d/%d)\n", uptime(), id, i, CYCLES);
        pause(3);

        // --- HUNGRY ---
        printf("[%d] Philosopher %d: HUNGRY\n", uptime(), id);

        // Acquire forks according to global index order
        sem_down(first_fork);
        sem_down(second_fork);

        // --- EATING ---
        printf("[%d] Philosopher %d: EATING with forks %d and %d\n", uptime(), id, first_fork, second_fork);
        pause(4); // Simulate eating

        // Release forks
        sem_up(second_fork);
        sem_up(first_fork);

        printf("[%d] Philosopher %d: FINISHED EATING\n", uptime(), id);
    }

    printf("[%d] Philosopher %d: DONE ALL CYCLES\n", uptime(), id);
    exit(0);
}

int main(int argc, char *argv[]) {
    // Initialize one binary semaphore per fork
    for (int i = 0; i < NUM_PHIL; i++) {
        sem_init(i, 1);
    }

    printf("Starting Dining Philosophers (5 Philosophers, %d Cycles each)...\n", CYCLES);

    // Spawn 5 philosopher processes
    for (int i = 0; i < NUM_PHIL; i++) {
        if (fork() == 0) {
            philosopher(i);
        }
    }

    // Parent waits for all 5 child processes to terminate
    for (int i = 0; i < NUM_PHIL; i++) {
        wait(0);
    }

    printf("All philosophers completed successfully. No deadlock occurred.\n");
    exit(0);
}