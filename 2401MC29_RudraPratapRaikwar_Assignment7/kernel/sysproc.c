#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

extern uint64 shm_page_pa;

uint64
sys_exit(void)
{
  int status;

  argint(0, &status);
  kexit(status);
  return 0;
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 addr;

  argaddr(0, &addr);
  return kwait(addr);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int lazy, n;

  argint(0, &n);
  argint(1, &lazy);
  addr = myproc()->sz;
  if (lazy == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0)
      return -1;
  } else {
    if (addr + n < addr || addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

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

uint64
sys_sleep(void)
{
  return sys_pause();
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_shm_get(void)
{
  struct proc *p = myproc();
  pte_t *pte = walk(p->pagetable, SHM_VA, 0);

  if (pte && (*pte & PTE_V))
    return SHM_VA;
  if (mappages(p->pagetable, SHM_VA, PGSIZE, shm_page_pa,
               PTE_R | PTE_W | PTE_U) < 0)
    return 0;
  return SHM_VA;
}

struct semaphore {
  int value;
  int initialized;
  struct spinlock lk;
};

static struct semaphore semaphores[10];

void
seminit(void)
{
  for (int i = 0; i < NELEM(semaphores); i++) {
    initlock(&semaphores[i].lk, "semaphore");
    semaphores[i].value = 0;
    semaphores[i].initialized = 0;
  }
}

uint64
sys_sem_init(void)
{
  int id, value;

  argint(0, &id);
  argint(1, &value);
  if (id < 0 || id >= NELEM(semaphores) || value < 0)
    return -1;
  acquire(&semaphores[id].lk);
  semaphores[id].value = value;
  semaphores[id].initialized = 1;
  release(&semaphores[id].lk);
  return 0;
}

uint64
sys_sem_wait(void)
{
  int id;

  argint(0, &id);
  if (id < 0 || id >= NELEM(semaphores))
    return -1;
  acquire(&semaphores[id].lk);
  if (!semaphores[id].initialized) {
    release(&semaphores[id].lk);
    return -1;
  }
  while (semaphores[id].value == 0) {
    sleep_prepare(&semaphores[id]);
    release(&semaphores[id].lk);
    sleep();
    acquire(&semaphores[id].lk);
    if (killed(myproc())) {
      release(&semaphores[id].lk);
      return -1;
    }
  }
  semaphores[id].value--;
  release(&semaphores[id].lk);
  return 0;
}

uint64
sys_sem_signal(void)
{
  int id;

  argint(0, &id);
  if (id < 0 || id >= NELEM(semaphores))
    return -1;
  acquire(&semaphores[id].lk);
  if (!semaphores[id].initialized) {
    release(&semaphores[id].lk);
    return -1;
  }
  semaphores[id].value++;
  wakeup(&semaphores[id]);
  release(&semaphores[id].lk);
  return 0;
}

uint64
sys_clone(void)
{
  uint64 fcn, arg, stack, returnaddr;

  argaddr(0, &fcn);
  argaddr(1, &arg);
  argaddr(2, &stack);
  argaddr(3, &returnaddr);
  return kclone(fcn, arg, stack, returnaddr);
}

uint64
sys_join(void)
{
  return kjoin();
}

uint64
sys_set_priority(void)
{
  int pid, priority;

  argint(0, &pid);
  argint(1, &priority);
  return ksetpriority(pid, priority);
}

uint64
sys_get_priority(void)
{
  int pid;

  argint(0, &pid);
  return kgetpriority(pid);
}
