// kernel/kalloc.c

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// Per-CPU free list structure
struct {
  struct {
    struct spinlock lock;
    struct run *freelist;
  } cpu[NCPU];
} kmem;

// This function is now only for initializing locks
void
kinit()
{
  char lock_name[16];
  for (int i = 0; i < NCPU; i++) {
    // Each lock gets a unique name, e.g., "kmem_0", "kmem_1"
    snprintf(lock_name, sizeof(lock_name), "kmem_%d", i);
    initlock(&kmem.cpu[i].lock, lock_name);
  }
}

// This is the new entry point for initialization from main.c
void
kmeminit()
{
  kinit();
  freerange(end, (void*)PHYSTOP);
}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}


void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // Get current CPU ID safely
  push_off();
  int cid = cpuid();
  pop_off();
  
  // Add the page to the current CPU's free list
  acquire(&kmem.cpu[cid].lock);
  r->next = kmem.cpu[cid].freelist;
  kmem.cpu[cid].freelist = r;
  release(&kmem.cpu[cid].lock);
}


// 在 kernel/kalloc.c 中替换 kalloc 函数

void *
kalloc(void)
{
  struct run *r;

  // Get current CPU ID safely
  push_off();
  int cid = cpuid();
  pop_off();

  // 1. Try to allocate from local CPU's free list (fast path)
  acquire(&kmem.cpu[cid].lock);
  r = kmem.cpu[cid].freelist;
  if(r) {
    kmem.cpu[cid].freelist = r->next;
  }
  release(&kmem.cpu[cid].lock);

  // 2. If local list was empty, try to steal from other CPUs (slow path)
  if(!r) {
    for(int i = 0; i < NCPU; i++) {
      if(i == cid) {
        continue;
      }
      
      acquire(&kmem.cpu[i].lock);
      r = kmem.cpu[i].freelist;
      if(r) {
        // We found a page to steal. Take it.
        kmem.cpu[i].freelist = r->next;
        release(&kmem.cpu[i].lock);
        // We successfully stole a page, so immediately stop searching.
        goto found; 
      }
      release(&kmem.cpu[i].lock);
    }
  }

found:
  if(r) {
    // Fill with junk *after* we have definitively acquired the page
    // and are no longer holding any other CPU's lock.
    memset((char*)r, 5, PGSIZE);
  }
    
  return (void*)r;
}
