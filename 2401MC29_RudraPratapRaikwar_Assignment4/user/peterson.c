#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct shared_data {
    volatile int flag[2];
    volatile int turn;
    volatile int shared_counter;
};

int main() {
    // 1. Request shared memory page
    struct shared_data *shm = (struct shared_data *) shm_get();
    if ((uint64)shm == 0) {
        printf("Error: shm_get failed\n");
        exit(1);
    }

    // Initialize variables (parent does this before forking)
    shm->flag[0] = 0;
    shm->flag[1] = 0;
    shm->turn = 0;
    shm->shared_counter = 0;

    // 2. Fork child
    int pid = fork();
    if (pid < 0) {
        printf("Error: fork failed\n");
        exit(1);
    }

    int id = (pid == 0) ? 1 : 0; // Process 0 = Parent, Process 1 = Child
    int other = 1 - id;

    for (int i = 0; i < 10; i++) {
        // --- ENTRY SECTION ---
        shm->flag[id] = 1;
        shm->turn = other;
        
        // Memory barrier to prevent out-of-order execution in RISC-V
        __sync_synchronize(); 
        
        // Busy wait
        while (shm->flag[other] == 1 && shm->turn == other) {
            // Spin
        }

        // --- CRITICAL SECTION ---
        int temp = shm->shared_counter;
        sleep(2); // Sleep forces a context switch, testing mutual exclusion
        temp++;
        shm->shared_counter = temp;
        
        printf("Process %d in CS, counter = %d\n", id, shm->shared_counter);

        // --- EXIT SECTION ---
        __sync_synchronize();
        shm->flag[id] = 0;

        // --- REMAINDER SECTION ---
        sleep(1);
    }

    // Parent waits for child to finish and prints the final result
    if (pid > 0) {
        wait(0);
        printf("Final counter value: %d\n", shm->shared_counter);
    }

    exit(0);
}