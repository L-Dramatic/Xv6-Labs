#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  
   backtrace();

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
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

// Set an alarm for the current process.
uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler_addr;
  struct proc *p = myproc();

  // Get arguments from user space
  if (argint(0, &interval) < 0 || argaddr(1, &handler_addr) < 0) {
    return -1;
  }

  // Store them in the process structure
  p->alarm_interval = interval;
  p->alarm_handler = (void (*)())handler_addr;
  p->ticks_left = interval; // Initialize the countdown

  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  
  // 1. 从备份中恢复 trapframe (恢复现场)
  *(p->trapframe) = *(p->saved_trapframe);

  // 2. 重置 alarm 倒计时
  p->ticks_left = p->alarm_interval;

  // 3. 解除 alarm 的激活状态
  p->alarm_active = 0;

  return 0; // The return value doesn't matter much.
}
