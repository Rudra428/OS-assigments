# OS Lab — Assignment 6:

**Student Name:** Rudra Pratap Raikwar  
**Roll Number:** 2401MC29  

## Assignment Overview
This assignment focuses on handling deadlocks in operating systems through four distinct strategies: Avoidance, Detection, Prevention, and Multi-Resource Synchronization. All solutions were implemented as user-space programs within the xv6-riscv environment.

---

### Question 1: Banker's Algorithm (Deadlock Avoidance)
I implemented the classic Banker's Algorithm (`bankers.c`) to dynamically avoid deadlocks by analyzing resource requests before they are granted. Modeling a system with 5 processes and 3 resource types, the program calculates the `Need` matrix at runtime. It uses the Safety Algorithm to verify if a safe execution sequence exists, and a Resource Request Algorithm that performs trial allocations. The program demonstrates two scenarios: it successfully processes a safe request by committing the allocation, and it properly denies an unsafe request by rolling back the temporary allocation to guarantee the system remains in a safe state.

### Question 2: Deadlock Detection using Wait-For Graphs
I implemented a graph-theory-based deadlock detection tool (`deadlockdetect.c`). The program takes standard Allocation and Request matrices and dynamically translates them into a directed Wait-For Graph, where an edge indicates that one process is waiting for a resource held by another. I wrote a 3-color Depth-First Search (DFS) algorithm to traverse this graph and look for back-edges (cycles). When tested on two hardcoded system states, the program successfully confirms when a graph is acyclic (no deadlock). When a deadlock is present, it detects the circular wait and prints the exact chain of trapped processes (e.g., Task 0 -> Task 1 -> Task 2 -> Task 0).

### Question 3: Deadlock Prevention via Resource Ordering
I demonstrated physical deadlock creation and its mathematical prevention using real xv6 processes, shared memory, and semaphores (`resourceorder.c`). First, I intentionally created a deadlock by forking two processes and forcing a Circular Wait condition: Process A acquired Lock 1 then waited for Lock 2, while Process B acquired Lock 2 and waited for Lock 1. This caused the OS processes to hang indefinitely. To fix the issue, I applied the Hierarchical Allocation (Resource Ordering) strategy. By modifying the code so that all processes must request locks in the exact same global order (Lock 1 strictly before Lock 2), the circular wait condition was broken, allowing both processes to execute and complete successfully.

### Question 4: Combined Synchronization & Deadlock Avoidance
I solved a complex multi-resource synchronization problem involving 5 processes sharing limited physical resources (`syncdeadlock.c`). The program models shared resource pools representing 2 Printers, 1 Scanner, and 2 Disks, protected by counting semaphores. Each forked process requires a specific combination of 2 resources to perform its simulated work. To guarantee deadlock prevention in this environment, I enforced a strict global acquisition hierarchy (`Printer < Scanner < Disk`). Because no process is allowed to request a lower-tier resource while holding a higher-tier one, circular waits are structurally impossible. The final program successfully coordinates all 5 processes across multiple iterations without any process getting permanently blocked.