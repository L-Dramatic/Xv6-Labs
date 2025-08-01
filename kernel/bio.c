// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 // Use a prime number for the number of buckets

// Simple hash function
#define BUCKET_HASH(blockno) (blockno % NBUCKETS)

struct {
  struct spinlock lock; // A global lock for eviction logic
  struct buf buf[NBUF];
  
  // Hash table with a lock per bucket
  struct {
    struct spinlock lock;
    struct buf head;
  } buckets[NBUCKETS];

} bcache;

void
binit(void)
{
  struct buf *b;
  char lock_name[16];

  initlock(&bcache.lock, "bcache");

  // Initialize bucket locks and lists
  for (int i = 0; i < NBUCKETS; i++) {
    snprintf(lock_name, sizeof(lock_name), "bcache.bucket_%d", i);
    initlock(&bcache.buckets[i].lock, lock_name);
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Put all buffers in the first bucket's list initially.
  // bget will move them to the correct buckets on first use.
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket_idx = BUCKET_HASH(blockno);

  // 1. Is the block already cached?
  acquire(&bcache.buckets[bucket_idx].lock);
  for(b = bcache.buckets[bucket_idx].head.next; b != &bcache.buckets[bucket_idx].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[bucket_idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[bucket_idx].lock);

  // 2. Not cached. Need to recycle an unused buffer.
  // This part can be serialized with a global lock to simplify things.
  acquire(&bcache.lock);

  // Re-check if the block was cached by another process while we were waiting for the global lock.
  acquire(&bcache.buckets[bucket_idx].lock);
  for(b = bcache.buckets[bucket_idx].head.next; b != &bcache.buckets[bucket_idx].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[bucket_idx].lock);
      release(&bcache.lock); // Don't forget to release the global lock
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[bucket_idx].lock);
  
  // Find an unlocked, LRU buffer to recycle.
  struct buf *victim = 0;
  uint oldest_ts = -1;
  
  for (int i = 0; i < NBUCKETS; i++) {
    acquire(&bcache.buckets[i].lock);
    for (b = bcache.buckets[i].head.next; b != &bcache.buckets[i].head; b = b->next) {
      if (b->refcnt == 0 && (victim == 0 || b->timestamp < oldest_ts)) {
        victim = b;
        oldest_ts = b->timestamp;
      }
    }
    release(&bcache.buckets[i].lock);
  }

  if (victim == 0) {
    release(&bcache.lock);
    panic("bget: no buffers");
  }

  int old_bucket_idx = BUCKET_HASH(victim->blockno);

  // Acquire locks in a consistent order to avoid deadlock.
  // We already hold the global bcache.lock.
  if (old_bucket_idx < bucket_idx) {
    acquire(&bcache.buckets[old_bucket_idx].lock);
    acquire(&bcache.buckets[bucket_idx].lock);
  } else if (bucket_idx < old_bucket_idx) {
    acquire(&bcache.buckets[bucket_idx].lock);
    acquire(&bcache.buckets[old_bucket_idx].lock);
  } else { // both in the same bucket
    acquire(&bcache.buckets[bucket_idx].lock);
  }

  // Check victim again after acquiring locks
  if (victim->refcnt != 0) {
      // Victim was used. Release locks and retry.
      if (old_bucket_idx != bucket_idx) release(&bcache.buckets[old_bucket_idx].lock);
      release(&bcache.buckets[bucket_idx].lock);
      release(&bcache.lock);
      return bget(dev, blockno); // Retry
  }
  
  // Remove victim from its old bucket list
  victim->prev->next = victim->next;
  victim->next->prev = victim->prev;
  if (old_bucket_idx != bucket_idx) {
    release(&bcache.buckets[old_bucket_idx].lock);
  }

  // Assign new block info to the victim buffer
  victim->dev = dev;
  victim->blockno = blockno;
  victim->valid = 0;
  victim->refcnt = 1;
  
  // Add victim to the new bucket list
  victim->prev = &bcache.buckets[bucket_idx].head;
  victim->next = bcache.buckets[bucket_idx].head.next;
  bcache.buckets[bucket_idx].head.next->prev = victim;
  bcache.buckets[bucket_idx].head.next = victim;
  
  release(&bcache.buckets[bucket_idx].lock);
  release(&bcache.lock);

  acquiresleep(&victim->lock);
  return victim;
}

struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_idx = BUCKET_HASH(b->blockno);
  acquire(&bcache.buckets[bucket_idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // If no one is using it, update its timestamp for LRU
    b->timestamp = ticks;
  }
  release(&bcache.buckets[bucket_idx].lock);
}

void
bpin(struct buf *b) {
  int bucket_idx = BUCKET_HASH(b->blockno);
  acquire(&bcache.buckets[bucket_idx].lock);
  b->refcnt++;
  release(&bcache.buckets[bucket_idx].lock);
}

void
bunpin(struct buf *b) {
  int bucket_idx = BUCKET_HASH(b->blockno);
  acquire(&bcache.buckets[bucket_idx].lock);
  b->refcnt--;
  release(&bcache.buckets[bucket_idx].lock);
}
