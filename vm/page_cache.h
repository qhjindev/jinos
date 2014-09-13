#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "types.h"
#include "buf.h"
#include "string.h"
#include "fs.h"
#include "mmu.h"

//class inode;

class page_t {
public:
  page_t* next;
  page_t* prev;

  inode* vnode;
  uint offset;
  page_t* next_hash;
  page_t** pprev_hash;
  page_t* vpnext;
  page_t* vpprev;
  void* vaddr;

  //buf_t* buffers;

  void init() {
    memset(this, 0, sizeof(struct page_t));
  }

  //buf_t* create_buffers(page_t* page, unsigned long size);
  //void create_empty_buffers(page_t* page, uint dev);
};

#define PAGE_HASH_BITS 11
#define PAGE_HASH_SIZE (1 << PAGE_HASH_BITS)

static inline unsigned long _page_hashfn(inode * ip, unsigned long offset)
{
#define i (((unsigned long) ip)/(sizeof(inode) & ~ (sizeof(inode) - 1)))
#define o (offset >> 12)
#define s(x) ((x)+((x)>>PAGE_HASH_BITS))
  return s(i+o) & (PAGE_HASH_SIZE-1);
#undef i
#undef o
#undef s
}

#define page_hash(inode,offset) (page_hash_table+_page_hashfn(inode,offset))

class page_cache_t {
public:
  void init();

  void hashin(page_t* page, inode* vp, uint offset);

  void hashout(page_t* page);

  page_t* find_page(inode* ip, uint offset);

  page_t* get_page(inode* ip, uint offset);

private:
  page_t** page_hash_table;
};

page_t* pte_page(pte_t pte);

extern page_cache_t page_cache;

#endif //VM_PAGE_H
