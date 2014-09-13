/*
 * kalloc.cpp
 *
 */

// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "kmem.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "page_cache.h"
#include "console.h"
#include "string.h"

kmem_t kmem;
extern char end[]; // first address after kernel loaded from ELF file

page_t* pages;
page_t* epages;

#define LONG_ALIGN(x) (((x)+(sizeof(long))-1)&~((sizeof(long))-1))

char* kmem_t::init() {

  int start_mem = PGALIGN((int)end);
  pages = (page_t*)LONG_ALIGN(start_mem);

  int maxpfn =  (PHYSTOP) >> PGSHIFT;
  epages = pages + maxpfn;
  return (char*)(epages);
}

void kmem_t::init_range(void* vstart, void* vend) {
  vstart = (char*)PGROUNDUP((uint)vstart);
  vend = (char*)PGROUNDUP((uint)vend);
  int startpfn = MAP_NR(vstart);
  int endpfn = MAP_NR(vend);
  int j=0;
  for(int i = startpfn; vstart < vend; i++, j++) {
    page_t* page = pages + i;
    page->vaddr = (void*)vstart;
    free_page(page);
    vstart += PGSIZE;
  }
}

void
kmem_t::free(char *v)
{
  unsigned long map_nr = MAP_NR(v);
  free_page(pages + map_nr);
}

void
kmem_t::free_page(page_t* p) {

  p->next = freelist;
  freelist = p;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
page_t*
kmem_t::alloc_page(void)
{
  page_t *i;

  i = freelist;
  if(i)
    freelist = i->next;

  memset(i->vaddr, 0, PGSIZE);

  return i;
}

char*
kmem_t::alloc(void)
{
  page_t *i;

  i = freelist;
  if(i)
    freelist = i->next;

  memset(i->vaddr, 0, PGSIZE);

  return (char*)i->vaddr;
}

