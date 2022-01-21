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

struct {
  struct spinlock lock;
  int ref_arr[PHYSTOP/PGSIZE];
} references;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


void increase(uint64 pa) {
  //printf("incr\n");
  acquire(&references.lock);
  int index=(pa-KERNBASE)/PGSIZE;
  if (pa>=PHYSTOP) {
    panic("invalid physical address");
  }
  if (references.ref_arr[index]<1) {
    panic("initialized pages cannot have a reference count of less than 1");
  }
  references.ref_arr[index]++;
  release(&references.lock);
}

void
kinit()
{ 
  //printf("kinit\n");
  initlock(&references.lock,"references");
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
    printf("freerange\n");
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    acquire(&references.lock);
    references.ref_arr[(((uint64)p-KERNBASE)/PGSIZE)]=1;//by initializing reference counter to 1, the kfree will send all the physical addresses to the freelist and will set the reference counter to 0(as it should be)
    release(&references.lock);
    kfree(p);
  } 
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  //printf("kfree\n");

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  

  r = (struct run*)pa;

  int index=(((uint64)pa-KERNBASE)/PGSIZE);//get the index i of the i-th page which corresponds to the specific physical address 

  acquire(&references.lock);
  //printf("lock acq");
  if (references.ref_arr[index]<1) {
    panic("kfree: reference counter cannot be less than one");
  }
  references.ref_arr[index]--;
  int temp=references.ref_arr[index];//reduce reference counter by one  
  release(&references.lock);
  if (temp==0) {//if no proccess "uses" this physicall address then we should add it to the freelist
    memset(pa, 1, PGSIZE);
   //printf("try lck ");
    acquire(&kmem.lock);
    //printf("lock axq ");
    
    r->next=kmem.freelist;//append the first element of the already existing list as the next entry of our physicall address struct run 
    kmem.freelist=r;//set the first element of the freelist to be the physical address we just freed
    release(&kmem.lock);
  } 
  
  
   
   
  
  //release(&references.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  //printf("kalloc\n");
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  //release(&kmem.lock);

  if(r) {
    acquire(&references.lock);
    references.ref_arr[((uint64)r-KERNBASE)/PGSIZE]=1;//initialize reference counter to one 
    release(&references.lock);
  }
  

  if(r) {
    //acquire(&kmem.lock);
    kmem.freelist = r->next;
    //release(&kmem.lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  release(&kmem.lock);
  //printf("kalloc end");
  return (void*)r;
  
}
