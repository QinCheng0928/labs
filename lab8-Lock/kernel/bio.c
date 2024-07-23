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

/*
原版 xv6 的设计中，使用双向链表存储所有的区块缓存

新的改进方案，可以建立一个从 blockno 到 buf 的哈希表，并为每个桶单独加锁。
仅有在两个进程同时访问的区块同时哈希到同一个桶的时候，才会发生锁竞争。
当桶中的空闲 buf 不足的时候，从其他的桶中获取 buf。
*/


// struct
// {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

//13 NBUCKET 个桶
struct bucket
{
  struct spinlock lock;
  struct buf head;
} hashtable[NBUCKET];
//哈希函数，根据blockno值区分放入哪个桶
int hash(uint dev, uint blockno)
{
  return blockno % NBUCKET;
}

/*
initlock 用于初始化一个普通的旋转锁（spinlock）。旋转锁是一种自旋等待的锁，
线程在获取锁时会不断地检查锁的状态，直到成功获取锁或被其他机制中断。

initsleeplock 用于初始化一个睡眠锁（sleeplock）。睡眠锁是一种阻塞等待的锁，
当线程无法获取锁时，它会进入睡眠状态，释放 CPU 给其他线程。
*/
void binit(void)
{
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  // {
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

  struct buf *b;
  initlock(&bcache.lock, "bcache");

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    initsleeplock(&b->lock, "buffer");
  }
  b = bcache.buf;
  //两层循环
  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&hashtable[i].lock, "bcache_bucket");
    //NBUF / NBUCKET == 3
    for (int j = 0; j < NBUF / NBUCKET; j++)
    {
      b->blockno = i; 
      b->next = hashtable[i].head.next;
      hashtable[i].head.next = b;
      b++;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  // struct buf *b;
  // acquire(&bcache.lock);
  // // block是否已经在cache中
  // for (b = bcache.head.next; b != &bcache.head; b = b->next)
  // {
  //   if (b->dev == dev && b->blockno == blockno)
  //   {
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // // cache中未找到
  // // Recycle the least recently used (LRU) unused buffer.
  // for (b = bcache.head.prev; b != &bcache.head; b = b->prev)
  // {
  //   if (b->refcnt == 0)
  //   {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");
  // printf("dev: %d blockno: %d Status: ", dev, blockno);

  struct buf *b;

  int idx = hash(dev, blockno);
  struct bucket *bucket = hashtable + idx;
  acquire(&bucket->lock);

  // block是否已经在cache中
  for (b = bucket->head.next; b != 0; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // cache中未找到
  // First try to find in current bucket.
  //在当前哈希桶中查找一个 未被引用且时间戳最小 的缓冲区来优化选择过程，
  //如果在当前哈希桶中找不到合适的缓冲区，则会在整个缓冲区缓存中进行全局查找。
  int min_time = 0x8fffffff;
  struct buf *replace_buf = 0;

  //遍历对应的桶
  for (b = bucket->head.next; b != 0; b = b->next)
  {
    if (b->refcnt == 0 && b->timestamp < min_time)
    {
      replace_buf = b;
      min_time = b->timestamp;
    }
  }
  if (replace_buf)
  {
    goto find;
  }

//桶内为找到替换，全局找
  acquire(&bcache.lock);
refind:
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    if (b->refcnt == 0 && b->timestamp < min_time)
    {
      replace_buf = b;
      min_time = b->timestamp;
    }
  }
  if (replace_buf)
  {
    int ridx = hash(replace_buf->dev, replace_buf->blockno);
    acquire(&hashtable[ridx].lock);
    if (replace_buf->refcnt != 0)
    {
      release(&hashtable[ridx].lock);
      goto refind;
    }
    //移动buf到对应的桶
    struct buf *pre = &hashtable[ridx].head;
    struct buf *p = hashtable[ridx].head.next;
    while (p != replace_buf)
    {
      pre = pre->next;
      p = p->next;
    }
    pre->next = p->next;
    release(&hashtable[ridx].lock);

    replace_buf->next = hashtable[idx].head.next;
    hashtable[idx].head.next = replace_buf;
    release(&bcache.lock);

    goto find;
  }
  else
  {
    panic("bget: no buffers");
  }

//找到替换
find:
  replace_buf->dev = dev;
  replace_buf->blockno = blockno;
  replace_buf->valid = 0;
  replace_buf->refcnt = 1;
  release(&bucket->lock);
  acquiresleep(&replace_buf->lock);
  return replace_buf;
}

// Return a locked buf with the contents of the indicated block.
//在读写硬盘的时候，需要通过 bread() 函数得到相应的缓存（缓存中已经存放了硬盘对应块中的数据）
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  // if(!holdingsleep(&b->lock))
  //   panic("brelse");
  // releasesleep(&b->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  // release(&bcache.lock);

  if (!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  int idx = hash(b->dev, b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  release(&hashtable[idx].lock);
}

void bpin(struct buf *b)
{
  // acquire(&bcache.lock);
  // b->refcnt++;
  // release(&bcache.lock);

  int idx = hash(b->dev, b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt++;
  release(&hashtable[idx].lock);
}

void bunpin(struct buf *b)
{
  // acquire(&bcache.lock);
  // b->refcnt--;
  // release(&bcache.lock);
  int idx = hash(b->dev, b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt--;
  release(&hashtable[idx].lock);
}
