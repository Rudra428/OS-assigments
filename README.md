# OS Assignment 4 - Process Synchronization

**Name**: Rudra Pratap Raikwar  
**Roll Number**: 2401MC29  
**Environment**: xv6-riscv on QEMU via WSL/Ubuntu  

## 1. Overview
This repository contains the implementation of classic concurrency problems utilizing custom synchronization primitives inside the xv6 operating system.

## 2. Design & Implementation Details

### Question 1: Peterson's Algorithm (`peterson.c`)
* **Shared State**: A new kernel system call `sys_shm_get` was implemented and mapped in `vm.c`. It allocates a physical page that persists across `fork()`, bypassing xv6's default isolated address spaces to allow parent and child processes to share state.
* **Logic**: The shared memory page houses the `flag` and `turn` variables. Strict mutual exclusion is enforced via busy-waiting, successfully protecting a shared counter without utilizing OS-level locks.

### Question 2: Producer-Consumer Problem (`prodcons.c`)
* **Synchronization**: Lightweight counting semaphores were implemented via new system calls (`sys_sem_init`, `sys_sem_down`, `sys_sem_up`). These are backed by xv6's internal `sleep()` and `wakeup()` functions to handle process blocking.
* **Logic**: A circular buffer of size 5 resides in the shared memory page. It is safely accessed by producer and consumer processes using counting semaphores (`empty`, `full`) and a binary mutex lock.

### Question 3: Readers-Writers Problem (`readwrite.c`)
* **Logic**: To prevent writer starvation, a fair turnstile synchronization approach was used instead of a strict reader-priority approach. This allows concurrent reads while ensuring waiting writers eventually get exclusive access. The shared data is protected using the custom semaphore implementation.

### Question 4: Dining Philosophers (`dining.c`)
* **Deadlock Avoidance**: Deadlock is avoided using Strict Resource Hierarchy (Resource Ordering). Philosophers must request their required forks in ascending numerical order. By forcing all philosophers to pick the lower-indexed fork first, the circular wait condition is entirely broken, guaranteeing deadlock-free execution.

## 3. Build and Execution Instructions
To compile the kernel and run the test programs:

1. Open your terminal in the root xv6 directory.
2. Compile and boot the OS by running:
   ```bash
   make qemu
