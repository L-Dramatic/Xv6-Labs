#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h" 

uint64 sys_mmap(void) {
  uint64 addr;
  int length, prot, flags, fd, offset;
  
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 || 
     argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0)
    return -1;
  
  struct proc *p = myproc();
  struct file *f = p->ofile[fd];
  
  if(f == 0 || (f->readable == 0 && (prot & PROT_READ)) || 
     (f->writable == 0 && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE)))
    return -1;

  struct vma *v = 0;
  for(int i = 0; i < NVMA; i++) {
    if(p->vma[i].used == 0) {
      v = &p->vma[i];
      break;
    }
  }
  if(v == 0)
    return -1;

  if(addr == 0) {
    addr = p->sz;
    p->sz += length;
  }
  

  v->used = 1;
  v->addr = addr;
  v->length = length;
  v->prot = prot;
  v->flags = flags;
  v->file = filedup(f);
  v->offset = offset;
  
  return addr;
}

uint64 sys_munmap(void) {
  uint64 addr;
  int length;
  
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  
  if(addr % PGSIZE != 0)
    return -1;
  
  struct proc *p = myproc();
  struct vma *vma = 0;

  for(int i = 0; i < NVMA; i++) {
    if(p->vma[i].used && addr >= p->vma[i].addr && addr < p->vma[i].addr + p->vma[i].length) {
      vma = &p->vma[i];
      break;
    }
  }
  
  if(vma == 0)
    return -1;

  if((vma->flags & MAP_SHARED) && (vma->prot & PROT_WRITE)) {
    filewrite(vma->file, addr, length);
  }

  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);

  if(addr == vma->addr && length == vma->length) {

    fileclose(vma->file);
    vma->used = 0;
  } else if(addr == vma->addr) {

    vma->addr += length;
    vma->length -= length;
    vma->offset += length;
  } else if(addr + length == vma->addr + vma->length) {

    vma->length -= length;
  } else {

    return -1;
  }
  
  return 0;
}
