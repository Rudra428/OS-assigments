#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BUF_SIZE 5
#define ITERS 20

// Semaphore IDs
#define MUTEX 0
#define EMPTY 1
#define FULL 2

// Shared memory layout
struct shared_data {
    int buf[BUF_SIZE];
    int in;
    int out;
};

int main() {
    // 1. Get true shared memory using our Q1 syscall
    struct shared_data *shm = (struct shared_data *) shm_get();
    if ((uint64)shm == 0) {
        printf("Error: shm_get failed\n");
        exit(1);
    }

    shm->in = 0;
    shm->out = 0;

    // 2. Initialize Semaphores
    sem_init(MUTEX, 1);          // Mutex lock for buffer access
    sem_init(EMPTY, BUF_SIZE);   // Tracks empty slots (starts at 5)
    sem_init(FULL, 0);           // Tracks filled slots (starts at 0)

    // 3. Fork into Producer and Consumer
    int pid = fork();
    if (pid < 0) {
        printf("Error: fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // --- CONSUMER ---
        for (int i = 0; i < ITERS; i++) {
            sem_down(FULL);      // Block if 0 full slots
            sem_down(MUTEX);     // Lock buffer

            int item = shm->buf[shm->out];
            printf("Consumer consumed: %d\n", item);
            shm->out = (shm->out + 1) % BUF_SIZE;

            sem_up(MUTEX);       // Unlock buffer
            sem_up(EMPTY);       // Signal an empty slot was created

            sleep(1);            // Small delay to simulate work
        }
    } else {
        // --- PRODUCER ---
        for (int i = 1; i <= ITERS; i++) {
            sem_down(EMPTY);     // Block if 0 empty slots
            sem_down(MUTEX);     // Lock buffer

            shm->buf[shm->in] = i;
            printf("Producer produced: %d\n", i);
            shm->in = (shm->in + 1) % BUF_SIZE;

            sem_up(MUTEX);       // Unlock buffer
            sem_up(FULL);        // Signal a full slot was created

            sleep(2);            // Producer is slower than consumer here
        }
        
        wait(0); // Wait for consumer to finish
        printf("Production and Consumption complete.\n");
    }

    exit(0);
}