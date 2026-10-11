# Operating Systems Assignment 7


## Q1 - User threads

`clone(fcn, arg, stack)` creates a schedulable process entry with a unique PID,
kernel stack, trapframe, file references, and parent relationship. Its user
registers start at `fcn(arg)` with the supplied stack top, and a small
userspace return trampoline calls `exit(0)` if the start routine returns.
`join()` waits for and reaps only clone-created children; `wait()` continues to
reap process children. Orphaned threads are adopted by init and can be reaped
there. Thread exit releases its page references rather than freeing pages
still used by siblings.

The trampoline uses the fixed `TRAPFRAME` virtual address, so each thread has a
private page-table root for its trapframe/trampoline mappings. The ordinary
user mappings refer to the same physical pages, with allocator reference
counts keeping shared pages alive until the last mapping is removed. This
preserves shared user-memory behavior while keeping trapframes isolated.

`threadtest` starts three workers, each performing 100,000 unsynchronized
increments. A short periodic pause widens the load/store race window so the
demonstration is observable rather than dependent on a particular emulator
interleaving. One QEMU run reported:

```text
threadtest: counter=102121 expected=300000
```

The value varies between runs; the important result is that all workers see
and modify one shared counter, while their unsynchronized read/modify/write
operations lose updates.

## Q2 - Semaphores and producer-consumer

The kernel maintains ten semaphore slots, each with a value, initialized
state, and spinlock. `sem_wait` sleeps on the semaphore channel while its
value is zero, then decrements it under the lock. `sem_signal` increments the
value and wakes sleepers. Invalid IDs, negative initial values, and use
before initialization return an error. The older `sem_down`/`sem_up` names
remain aliases for compatibility with existing user programs.

`synctest` serializes each shared-counter increment with a semaphore, then
uses `empty`, `full`, and `mutex` semaphores around a five-slot circular
buffer. Its output showed:

```text
synctest: counter=300000 expected=300000
producer: 1
consumer: 1
...
producer: 20
consumer: 20
synctest: producer-consumer complete
```

The producer and consumer may interleave differently on each run, but the
semaphores prevent reading an empty buffer or writing a full one.

## Q3 - Deadlock detection and avoidance

`deadlock.c` uses fixed-size allocation/request/available arrays. The detector
repeatedly finds a process whose outstanding request fits in the current
available vector, marks it complete, and returns its allocation to the
working vector. Processes left unfinished are reported as deadlocked. The
Banker's safety check derives `Need = Max - Allocation`, finds a safe
completion sequence, and rejects a hypothetical request if the resulting
state has no such sequence.

The three simulated cases produced:

| Case | Result |
|---|---|
| Two processes each hold one resource and request the other | P0 and P1 deadlocked |
| Available resources allow P1 and then P0 to finish | No deadlock |
| Initially safe Banker state; grant P0 one unit of resource 0 | Request denied as unsafe |

The program also creates a real pipe wait cycle. One child retains Pipe1's
writer while reading Pipe2; the other retains Pipe2's writer while reading
Pipe1. The parent runs the detector on the corresponding resource state,
prints both blocked PIDs, then kills and reaps the children so the test does
not hang.

## Q4 - Priority scheduling and aging

Every process starts at priority 10; the accepted range is 1 (highest) through
20 (lowest). The scheduler selects the runnable process with the lowest
priority number. Its scan cursor advances after dispatch to give equal
priorities round-robin order. Every 30 ticks that a process remains runnable,
aging reduces its priority number by one, down to 1. `set_priority(pid, p)`
and `get_priority(pid)` expose this state to user programs.

In the QEMU run below, the priority-20 child was observed aging while the
higher-priority CPU-bound children ran:

```text
aging: lowest-priority child is now priority=20 after 30 ticks
aging: lowest-priority child is now priority=19 after 60 ticks
aging: lowest-priority child is now priority=18 after 90 ticks
child pid=17 finished priority=1 completion_tick=100 runtime=100
child pid=18 finished priority=5 completion_tick=101 runtime=100
child pid=19 finished priority=10 completion_tick=102 runtime=100
child pid=20 finished priority=17 completion_tick=103 runtime=100
priotest: all children completed; aging promotes waiting processes
```

| Initial priority | Completion tick in this run | Final priority |
|---:|---:|---:|
| 1 | 100 | 1 |
| 5 | 101 | 5 |
| 10 | 102 | 10 |
| 20 | 103 | 17 |

The measured completion ticks are illustrative and depend on QEMU's CPU count
and scheduling interleavings. The priority changes and completion of the
lowest-priority child demonstrate that it is not starved.

## Validation and submission evidence

The kernel and all four user programs compile, and the assignment programs
were run in QEMU. The `usertests -q` functional cases completed, including the
lazy-allocation tests; the harness's final free-page accounting reported one
fewer free page than its initial count. This remaining one-page discrepancy
is included here rather than presented as a clean regression-suite pass.

Terminal screenshot images are not embedded in this report. For submission,
run the commands above in the QEMU terminal and capture the four program
outputs; the text results in this report identify the expected output lines.
