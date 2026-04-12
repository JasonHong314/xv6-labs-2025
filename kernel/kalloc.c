#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PAGE_NUM ((PHYSTOP - KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int ref_cnt[PAGE_NUM];
} page_ref_cnt;

static inline uint64
page_index(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_ref_cnt.lock, "page_ref_cnt");

  for(int i = 0; i < PAGE_NUM; i++)
    page_ref_cnt.ref_cnt[i] = 0;

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // 先设成 1，再 kfree，让 kfree 把它减到 0 并真正放回 freelist
    acquire(&page_ref_cnt.lock);
    page_ref_cnt.ref_cnt[page_index((uint64)p)] = 1;
    release(&page_ref_cnt.lock);

    kfree(p);
  }
}

void
kfree(void *pa)
{
  struct run *r;
  uint64 idx;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  idx = page_index((uint64)pa);

  acquire(&page_ref_cnt.lock);
  if(page_ref_cnt.ref_cnt[idx] < 1)
    panic("kfree: ref_cnt");

  page_ref_cnt.ref_cnt[idx]--;

  if(page_ref_cnt.ref_cnt[idx] > 0){
    release(&page_ref_cnt.lock);
    return;
  }
  release(&page_ref_cnt.lock);

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE);

    acquire(&page_ref_cnt.lock);
    page_ref_cnt.ref_cnt[page_index((uint64)r)] = 1;
    release(&page_ref_cnt.lock);
  }

  return (void*)r;
}

void
k_page_ref_inc(uint64 pa)
{
  acquire(&page_ref_cnt.lock);
  page_ref_cnt.ref_cnt[page_index(pa)] += 1;
  release(&page_ref_cnt.lock);
}

void
k_page_ref_dec(uint64 pa)
{
  //kfree directly
  kfree((void*)pa);
}

int
k_page_ref(uint64 pa)
{
  int ref_c;

  acquire(&page_ref_cnt.lock);
  ref_c = page_ref_cnt.ref_cnt[page_index(pa)];
  release(&page_ref_cnt.lock);

  return ref_c;
}