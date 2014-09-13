/*
 * page_cache.cpp
 *
 *  Created on: Oct 12, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "param.h"
#include "fs.h"
#include "mmu.h"
#include "page_cache.h"
#include "kmem.h"


page_cache_t page_cache;

extern page_t* pages;
page_t* pte_page(pte_t pte) {
  int pfn = pte >> PGSHIFT;
  return pages + pfn;
}

/*
void page::create_empty_buffers(page_t* page, uint dev) {
  buf_t *b, *head, *tail;

  head = create_buffers(page, BSIZE);
  if(page->buffers)
    panic("page has buffers");

  b = head;
  do {
    b->dev = dev;
    tail = b;
    b = b->this_page;
  } while(b);
  tail->this_page = head;
  page->buffers = head;
}

buf_t*
page::create_buffers(page_t* page, unsigned long size) {
  buf_t *b, *head;
  long offset;

  head = NULL;
  offset = PGSIZE;
  while((offset -= size) >= 0) {
    b = bcache.get(0, 0);
    if(b == 0) {
      cprintf("no buf in create_buffers\n");
      return NULL;
    }

    b->this_page = head;
    head = b;
    //b->data = page->vaddr + offset;
  }

  return head;
}
*/

void page_cache_t::init() {
  page_hash_table = (page_t**) kmem.alloc();
}

void page_cache_t::hashin(page_t* page, inode* vp, uint offset) {

  page_t** phash = page_hash(vp, offset);

  page->offset = offset;

  //add page to inode queue
  page_t** ppages = &vp->pages;
  page->vnode = vp;
  page->prev = 0;
  if ((page->next = *ppages) != 0)
    page->next->prev = page;
  *ppages = page;

  //add page to page hash
  page_t* next = *phash;
  *phash = page;
  page->next_hash = next;
  page->pprev_hash = phash;
  if (next)
    next->pprev_hash = &page->next_hash;
}

void page_cache_t::hashout(page_t* page) {

  //remove page from hash table
  page_t *next = page->next_hash;
  page_t **pprev = page->pprev_hash;
  if (next)
    next->pprev_hash = pprev;
  *pprev = next;
  page->pprev_hash = 0;

  //remove page from inode queue
  struct inode * inode = page->vnode;
  page->vnode = 0;

  if (inode->pages == page)
    inode->pages = page->next;
  if (page->next)
    page->next->prev = page->prev;
  if (page->prev)
    page->prev->next = page->next;
  page->next = 0;
  page->prev = 0;
}

page_t* page_cache_t::find_page(inode* ip, uint offset) {
  page_t* page = *page_hash(ip, offset);

  goto inside;

  for (;;) {
    page = page->next_hash;
inside:
    if (!page)
      goto not_found;
    if (page->vnode != ip)
      continue;
    if (page->offset == offset)
      break;
  }

not_found:
  return page;
}

page_t*
page_cache_t::get_page(inode* ip, uint  offset) {
  page_t* page = find_page(ip, offset);
  if(page)
    return page;

  page = kmem.alloc_page();
  this->hashin(page, ip, offset);
  return page;
}

