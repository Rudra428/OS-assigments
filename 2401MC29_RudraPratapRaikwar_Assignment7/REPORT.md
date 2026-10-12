# Operating Systems Assignment 7 - Report

## Question 1 — Implementing User-Level Threads

**Design Decisions and Key Data Structures**
To implement basic kernel-supported threads, the standard process structure (`struct proc`) in `proc.h` was modified to include an `is_thread` flag. The `clone()` system call was designed to closely mirror `fork()`, but instead of duplicating the parent's page directory, it assigns the child's `pgdir` pointer to the parent's `pgdir`, enabling a shared address space. A separate kernel stack and trapframe are allocated for the thread. The `join()` system call was implemented to wait specifically for children marked as threads. Crucially, the standard `wait()` and `exit()` system calls were modified to ignore thread-specific cleanup, ensuring that a thread exiting does not inadvertently free the shared address space while other threads or the parent process are still utilizing it.

**Problems Faced and Resolution**
The primary challenge was managing memory deallocation upon thread exit. Initially, standard process reaping would attempt to free the page directory, causing kernel panics when sibling threads subsequently accessed memory. This was resolved by separating process and thread cleanup logic: `join()` frees the thread's kernel stack and state but leaves the `pgdir` intact, deferring address space deallocation to the parent process's final `wait()`. 

---

## Question 2 — Process/Thread Synchronization using Semaphores

**Design Decisions and Key Data Structures**
A new `struct semaphore` was introduced in kernel space, containing an integer `value`, an `active` state flag, and a `struct spinlock` to ensure atomic operations. A global, fixed-size array of 10 semaphores was allocated, protected by a global array lock during initialization. The `sem_wait()` and `sem_signal()` system calls were built on top of xv6's native `sleep()` and `wakeup()` primitives. When `sem_wait()` encounters a value <= 0, it drops the semaphore lock, sleeps on the semaphore channel, and reacquires the lock upon waking. 

**Problems Faced and Resolution**
A challenge encountered during the Producer-Consumer implementation was ensuring the CPU correctly yielded when the buffer was full or empty without causing race conditions during the sleep transition. This was resolved by carefully ordering the lock acquisitions and utilizing xv6's atomic sleep mechanism, which requires passing the semaphore's spinlock directly into `sleep()` so it can be released atomically as the process yields.

**Before/After Comparison of the Race Condition (Q1 vs Q2)**
*   **Before (Q1):** When running `threadtest`, 3 threads incremented a shared global counter 100,000 times each without synchronization. Due to concurrent read-modify-write operations interleaving at the assembly level, updates were lost. The final counter value fluctuated across runs but consistently settled far below the target (e.g., `counter=105299 expected=300000`).
*   **After (Q2):** Wrapping the critical section inside `synctest` with `sem_wait()` and `sem_signal()` enforced strict mutual exclusion. As a result, the race condition was entirely eliminated, yielding a perfect final value of `counter=300000` on every execution. Furthermore, the Producer-Consumer log verified that a 5-slot buffer was perfectly respected without underflow or overflow.

---

## Question 3 — Deadlock Detection and Avoidance

**Design Decisions and Key Data Structures**
Deadlock detection and the Banker's Algorithm were implemented entirely in user space utilizing fixed-size static arrays to represent system state: `Available[m]`, `Allocation[n][m]`, `Request[n][m]`, and `Max[n][m]`. The `detect_deadlock()` function iteratively searches for a process whose outstanding requests can be satisfied by the `Available` vector. If found, it simulates completion by adding `Allocation` to `Available`. Any processes remaining in an unfinished state at the end of the loop are flagged as deadlocked. 

**Problems Faced and Resolution**
Simulating a realistic deadlock in xv6 using actual processes required coordinating blocking states outside of standard arrays. This was resolved by utilizing xv6 pipes to create a literal circular wait: Process A was forced to hold Pipe 1's write end while waiting to read Pipe 2, and Process B held Pipe 2's write end while waiting to read Pipe 1. The detection algorithm accurately mapped this physical state and flagged the deadlock.

**Deadlock Detection Results across Test Cases**
1.  **Case 1 (Deadlock):** Initial state of `Available: 0 0` with crossed requests. The algorithm correctly halted and identified `P0` and `P1` as deadlocked.
2.  **Case 2 (Recoverable State):** Initial state of `Available: 1 0`. The algorithm determined `P1` could finish, bringing `Available` to `1 1`, which subsequently allowed `P0` to finish. Result: `deadlocked processes: none`.
3.  **Case 3 (Banker's Algorithm Avoidance):** Initial state proven safe (execution sequence `P1 -> P0`). A hypothetical request by `P0` for resources `1 0` was evaluated. The simulated resulting state failed the safety check, and the request was correctly denied as unsafe.

---

## Question 4 — Priority-Based Preemptive CPU Scheduling

**Design Decisions and Key Data Structures**
The default Round-Robin scheduler was replaced with a priority-based preemptive scheduler. A `priority` field (1 to 20) and a `wait_ticks` integer were added to `struct proc`. By default, processes are instantiated with a priority of 10. The `scheduler()` function in `proc.c` was rewritten to scan the entire process table and select the `RUNNABLE` process with the absolute lowest priority number. To prevent starvation, an aging mechanism was implemented: every time a process remains `RUNNABLE` for 30 consecutive ticks without being scheduled, its priority number is decremented by 1 (increasing its actual priority).

**Problems Faced and Resolution**
Ensuring equal-priority processes did not block each other required modifying the selection cursor. The scan loop was adjusted to continue from where it left off, breaking ties in round-robin order rather than always prioritizing the lowest PID. Testing aging effectively required generating purely CPU-bound tasks in `priotest.c` to guarantee the lower-priority processes were starved long enough to trigger the 30-tick aging threshold multiple times.

**Scheduling Behavior Table (Completion Time vs. Priority)**
The following table demonstrates the completion order of four distinct CPU-bound processes. The lowest-priority process (PID 20) started at priority 20 but successfully completed execution because the aging mechanism gradually promoted it to priority 17, entirely preventing starvation.

| Process ID | Initial Priority | Final Priority (After Aging) | Completion Tick |
|:---:|:---:|:---:|:---:|
| 17 | 1 (Highest) | 1 | 100 |
| 18 | 5 | 5 | 101 |
| 19 | 10 (Default) | 10 | 102 |
| 20 | 20 (Lowest) | 17 | 103 |
