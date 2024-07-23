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

struct run
{
  struct run *next;
};

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;

struct
{
  struct spinlock lock;
  struct run *freelist;
  char lock_name[7];
} kmem[NCPU];

void kinit()
{
  // initlock(&kmem.lock, "kmem");
  // freerange(end, (void*)PHYSTOP);

  for (int i = 0; i < NCPU; i++)
  {
    snprintf(kmem[i].lock_name, sizeof(kmem[i].lock_name), "kmem_%d", i);
    initlock(&kmem[i].lock, kmem[i].lock_name);
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  // struct run *r;
  // if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
  //   panic("kfree");
  // // Fill with junk to catch dangling refs.
  // memset(pa, 1, PGSIZE);
  // r = (struct run*)pa;
  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  // push_off(): 这个函数会禁用当前 CPU 的中断
  // pop_off(): 这个函数恢复 push_off() 之前的中断状态。
  // cpuid(): 这个函数返回当前执行代码的 CPU ID。
  push_off();
  uint cpu = cpuid();

  memset(pa, 1, PGSIZE);
  r = (struct run *)pa;
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  // struct run *r;
  // acquire(&kmem.lock);
  // r = kmem.freelist;
  // if (r)
  //   kmem.freelist = r->next;
  // release(&kmem.lock);
  // if (r)
  //   memset((char *)r, 5, PGSIZE); // fill with junk
  // return (void *)r;
  struct run *r;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);

  r = kmem[id].freelist;
  if (r)
  {
    kmem[id].freelist = r->next;
  }
  else//空闲列表为空
  {
    // stealing when a CPU's free list is empty
    // 窃取其他空闲列表
    int success = 0;
    int i = 0;
    for (i = 0; i < NCPU; i++)
    {
      if (i == id)  //是自己，跳过
        continue;
      acquire(&kmem[i].lock);

      struct run *p = kmem[i].freelist;
      if (p)
      {
        // 分一半空间
        struct run *fp = p; // 每次往后两个
        struct run *pre = p;
        while (fp && fp->next)
        {
          fp = fp->next->next;
          pre = p;
          p = p->next;
        }
        //新空闲链表执行窃取的空闲链表前半部分
        kmem[id].freelist = kmem[i].freelist;
        if (p == kmem[i].freelist)
        {
          // i只有一个
          kmem[i].freelist = 0;
        }
        else//第i空闲链表指向后半部分
        {
          kmem[i].freelist = p;
          pre->next = 0;
        }
        success = 1;
      }

      release(&kmem[i].lock);
      if (success)
      {
        r = kmem[id].freelist;
        kmem[id].freelist = r->next;
        break;
      }
    }
  }

  release(&kmem[id].lock);
  pop_off();
  if (r)
    memset((char *)r, 5, PGSIZE);
  return (void *)r;
}
