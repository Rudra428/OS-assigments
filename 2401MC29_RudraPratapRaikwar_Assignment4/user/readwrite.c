#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Semaphore IDs
#define MUTEX 0       // Protects the read_count variable
#define RW_LOCK 1     // Protects the shared_data (exclusive for writers)
#define TURNSTILE 2   // Ensures fairness so writers don't starve

struct shared_memory {
    int shared_data;
    int read_count;
};

void reader(struct shared_memory *shm, int reader_id) {
    for (int i = 0; i < 2; i++) {
        // --- ENTRY SECTION ---
        sem_down(TURNSTILE); // Wait in line (prevents starving the writer)
        sem_down(MUTEX);     // Lock the read_count variable
        
        shm->read_count++;
        if (shm->read_count == 1) {
            sem_down(RW_LOCK); // If first reader, lock out all writers
        }
        
        sem_up(MUTEX);       // Unlock read_count
        sem_up(TURNSTILE);   // Let the next process through the turnstile

        // --- CRITICAL SECTION (READING) ---
        // Multiple readers can be in here at the same time!
        printf("[%d] Reader %d (PID %d) read: %d | Active Readers: %d\n", 
               uptime(), reader_id, getpid(), shm->shared_data, shm->read_count);
        
        pause(4); // Simulate time taken to read (allows concurrent reads)

        // --- EXIT SECTION ---
        sem_down(MUTEX);     // Lock read_count
        shm->read_count--;
        if (shm->read_count == 0) {
            sem_up(RW_LOCK); // If last reader, wake up waiting writers
        }
        sem_up(MUTEX);       // Unlock read_count
        
        pause(2); // Rest before reading again
    }
    exit(0);
}

void writer(struct shared_memory *shm, int writer_id) {
    for (int i = 0; i < 2; i++) {
        // --- ENTRY SECTION ---
        sem_down(TURNSTILE); // Wait in line (blocks new readers from entering)
        sem_down(RW_LOCK);   // Wait until ALL active readers are completely done

        // --- CRITICAL SECTION (WRITING) ---
        // EXCLUSIVE ACCESS: No readers and no other writers can be here!
        shm->shared_data++;
        printf("[%d] Writer %d (PID %d) WROTE: %d | (Exclusive Access)\n", 
               uptime(), writer_id, getpid(), shm->shared_data);
        
        pause(8); // Simulate time taken to write

        // --- EXIT SECTION ---
        sem_up(RW_LOCK);     // Unlock data
        sem_up(TURNSTILE);   // Unlock turnstile so others can enter
        
        pause(3); // Rest before writing again
    }
    exit(0);
}

int main() {
    // 1. Get true shared memory
    struct shared_memory *shm = (struct shared_memory *) shm_get();
    if ((uint64)shm == 0) {
        printf("Error: shm_get failed\n");
        exit(1);
    }

    shm->shared_data = 0;
    shm->read_count = 0;

    // 2. Initialize Semaphores
    sem_init(MUTEX, 1);
    sem_init(RW_LOCK, 1);
    sem_init(TURNSTILE, 1);

    printf("Starting 3 Readers and 2 Writers...\n");

    // 3. Spawn 3 Readers
    for (int i = 1; i <= 3; i++) {
        if (fork() == 0) {
            reader(shm, i);
        }
    }
    
    // 4. Spawn 2 Writers
    for (int i = 1; i <= 2; i++) {
        if (fork() == 0) {
            writer(shm, i);
        }
    }

    // 5. Wait for all 5 child processes to finish
    for (int i = 0; i < 5; i++) {
        wait(0);
    }

    printf("All reading and writing complete. Final data value: %d\n", shm->shared_data);
    exit(0);
}