/*
 * seg_vnode.cpp
 *
 */

#include "types.h"
#include "errno.h"
#include "mmu.h"
#include "seg_vnode.h"
#include "fs.h"
#include "vas.h"
#include "hat.h"
#include "page_cache.h"
#include "kmem.h"
#include "mman.h"
#include "string.h"
#include "console.h"
#include "x86.h"
#include "memlayout.h"

void*
seg_vnode_t::operator new(uint size) {
  return kmem.alloc();
}

void
seg_vnode_t::operator delete(void* p) {
  kmem.free((char*)p);
}

int seg_vnode_t::dup(seg_t* oseg) {
  seg_vnode_t* oseg_vn = (seg_vnode_t*)oseg;
  this->vp = oseg_vn->vp;
  this->offset = oseg_vn->offset;
  this->prot = oseg_vn->prot;
  this->type = oseg->type;

  return this->vas->hat->dup(oseg->vas->hat, oseg);
}

int seg_vnode_t::unmap(int addr, uint len) {
  return 0;
}

page_t*
seg_vnode_t::file_fault(int addr) {

  unsigned long pgoff, endoff;

  pgoff = ((addr - base) >> PGSHIFT) + offset;
  endoff = (this->size >> PGSHIFT) + offset;

  page_t* page = page_cache.find_page(this->vp, pgoff);
  if(page)
    return 0;

  page = kmem.alloc_page();
  if(!page)
    return 0;

  page_cache.hashin(page, this->vp, pgoff);

  this->vp->read_page(page);

  return page;
}

int
seg_vnode_t::no_page(int addr, enum seg_rw rw, pte_t* page_table) {
  if(!this->vp)
    return anon_page(addr, rw, page_table);

  page_t* new_page = file_fault(addr);
  if((rw == S_WRITE) && (this->type == MAP_PRIVATE)) {
    page_t* page = kmem.alloc_page();
    memcpy(page->vaddr, new_page->vaddr, PGSIZE);
    new_page = page;
  }

  if(!*page_table) {
    int perm = PTE_U|PTE_D;
    if(rw == S_WRITE)
      perm |= PTE_W;
    hat_t::mappage(page_table, (uint)addr, V2P(new_page->vaddr), perm);
  }

  return 1;
}

int
seg_vnode_t::wp_page(int addr, pte_t* pte) {
  page_t* old_page = pte_page(*pte);
  page_t* new_page = kmem.alloc_page();
  if(!new_page) {
    cprintf("no mem in wp_page");
    return -1;
  }
  memcpy(new_page->vaddr, old_page->vaddr, PGSIZE);
  hat_t::mappage(pte, (uint)addr, V2P(new_page->vaddr), PTE_U|PTE_W|PTE_D);
  return 1;
}

int
seg_vnode_t::anon_page(int addr, enum seg_rw rw, pte_t* page_table) {

  //if(rw == S_WRITE)
  {
    page_t* page = kmem.alloc_page();
    if(!page)
      return -1;
    memset(page->vaddr, 0, PGSIZE);
    hat_t::mappage(page_table, (uint)addr, V2P(page->vaddr), PTE_U|PTE_W|PTE_D);
  }

  return 1;
}

faultcode_t
seg_vnode_t::fault(int addr, uint len, enum fault_type type, enum seg_rw rw) {

  vas_t* vas = this->vas;
  hat_t* hat = vas->hat;
  pte_t* pte = hat->walkpgdir(hat->get_pgdir(), (const void*)addr, 1);
  if(!pte)
    return -1;

  pte_t entry  = *pte;
  if(!(entry & PTE_P)) {
    if(!entry)
      return no_page(addr, rw, pte);
  }

  if(rw == S_WRITE) {
    int v = entry & PTE_W;
    if(!(entry & PTE_W))
      return wp_page(addr, pte);

    entry |= PTE_D;
  }

  entry |= PTE_A;
  flush_tlb();
  *pte = entry;
  return 0;
}



