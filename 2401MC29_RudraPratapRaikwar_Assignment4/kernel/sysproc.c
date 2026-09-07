#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

// Exit the current process. Retrieves the exit status from user space
// and calls the kernel's kexit() to terminate and clean up.
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

// Retrieves and returns the Process ID (PID) of the currently running process.
uint64
sys_getpid(void)
{
  return myproc()->pid;
}

// Spawns a new process by duplicating the calling process using kfork().
uint64
sys_fork(void)
{
  return kfork();
}

// Suspends execution of the calling process until one of its children terminates.
// Retrieves the memory address to store the child's exit status.
uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

// Adjusts the memory size of the process (sbrk). 
// Supports both eager allocation and lazy allocation based on the 't' flag.
uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz; // Current size of the process memory

  if (t == SBRK_EAGER || n < 0) {
    // Eagerly allocate or deallocate memory right now
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

// Pauses the execution of the current process for a specified number of clock ticks.
uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  
  acquire(&tickslock);
  ticks0 = ticks;
  
  // Loop and yield CPU until the requested number of ticks has passed
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

// Sends a kill signal to a process with the specified PID.
uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// =========================================================================
// CUSTOM SYSTEM CALL: shm_get
// Purpose: Maps a physical page into the calling process's high virtual 
// address space to allow true shared memory between isolated processes.
// =========================================================================
extern uint64 shm_page_pa;

uint64 sys_shm_get(void) {
  struct proc *p = myproc();
  uint64 va = MAXVA - 3 * PGSIZE; // Use an actually empty slot! (Avoids trapframe collisions)
  
  // Check if it's already mapped
  // This prevents panic("mappages: remap") if called multiple times
  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte != 0 && (*pte & PTE_V)) {
    return va;
  }
  
  extern uint64 shm_page_pa;
  
  // Map the pre-allocated physical page into the virtual address space
  // PTE_R (Read), PTE_W (Write), and PTE_U (User space access) flags set
  if(mappages(p->pagetable, va, PGSIZE, shm_page_pa, PTE_R | PTE_W | PTE_U) != 0)
    return 0;
    
  return va;
}

// =========================================================================
// CUSTOM SEMAPHORE IMPLEMENTATION
// Purpose: Provides lightweight counting semaphores utilizing xv6's native 
// sleep/wakeup primitives for the Producer-Consumer problem synchronization.
// =========================================================================

struct spinlock sem_lock[10];  // Array of locks to protect each semaphore count atomically
int sem_count[10];             // Array holding the actual counter values for each semaphore
int sem_initialized = 0;       // Flag tracking if the semaphores have been initialized

// Initialize a specific semaphore ID with a starting value
uint64 sys_sem_init(void) {
  int id, val;
  argint(0, &id);
  argint(1, &val);
  
  // Validate semaphore ID bounds
  if(id < 0 || id >= 10) return -1;
  
  // Initialize locks on the first call (lazy initialization)
  if(sem_initialized == 0) {
    for(int i = 0; i < 10; i++) {
      initlock(&sem_lock[i], "sem");
    }
    sem_initialized = 1;
  }
  
  acquire(&sem_lock[id]); // Lock to ensure atomic update
  sem_count[id] = val;    // Set the initial resource count
  release(&sem_lock[id]);
  
  return 0;
}

// The wait/P/down operation: decreases the count and blocks if resources are unavailable
uint64 sys_sem_down(void) {
  int id;
  argint(0, &id);
  
  if(id < 0 || id >= 10) return -1;

  acquire(&sem_lock[id]);
  
  // Block if the count is 0 or less
  // A while loop is used instead of 'if' to safely handle spurious wakeups
  while(sem_count[id] <= 0) {
    sleep_prepare(&sem_count[id]); // Register process to sleep on this specific channel
    release(&sem_lock[id]);        // Release lock before sleeping to avoid deadlock
    sleep();                 // Calls sleep() with no arguments, matching your kernel
    acquire(&sem_lock[id]);        // Re-acquire lock upon waking to check condition again
  }
  
  sem_count[id]--; // Successfully acquired the resource, decrement count
  release(&sem_lock[id]);
  
  return 0;
}

// The signal/V/up operation: increases the count and wakes sleeping processes
uint64 sys_sem_up(void) {
  int id;
  argint(0, &id);
  
  if(id < 0 || id >= 10) return -1;

  acquire(&sem_lock[id]); // Lock before modifying shared state
  
  sem_count[id]++;        // Resource released, increment count
  wakeup(&sem_count[id]); // Wake up any sleeping processes waiting on this channel
  
  release(&sem_lock[id]);
  
  return 0;
}