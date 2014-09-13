/*
 * seg.cpp
 *
 *  Created on: Oct 9, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "param.h"
#include "vas.h"
#include "seg_vnode.h"
#include "kmem.h"
#include "console.h"
#include "proc.h"


int
seg_t::dup(seg_t* newseg) {
  cprintf("call seg_t::dup");
  return -1;
}

int
seg_t::unmap(int addr, uint len) {
  cprintf("call seg_t::unmap");
  return -1;
}

faultcode_t
seg_t::fault(int addr, uint len, enum fault_type type, enum seg_rw rw) {
  cprintf("call seg_t::fault");
  return -1;
}

void
seg_t::operator delete(void* p) {
  kmem.free((char*)p);
}

seg_t*
seg_t::alloc(vas_t* vas, int base, uint size) {

  seg_vnode_t* newseg;
  int segbase;
  uint segsize;

  segbase = (int)((uint)base & PAGEMASK);
  segsize = ((uint)(base + size) + PAGEOFFSET) & PAGEMASK - (uint)segbase;

  proc_t* p = proc;

  //newseg = (seg_vnode_t*)kmem.alloc();
  newseg = new seg_vnode_t;
  if(newseg->attach(vas, segbase, segsize) < 0) {
    cprintf("newseg: base:%x size:%x attach to vas failed\n", segbase, segsize);
    //kmem.free((char*)newseg);
    //delete newseg;
    return (seg_t*) 0;
  }

  return newseg;
}

int  seg_t::attach(vas_t* vas, int base, uint size) {
  this->vas = vas;
  this->base = base;
  this->size = size;

  return vas->addseg(this);
}

void seg_t::free() {
  if(vas->segs == this)
    vas->segs = this->next;  /* go to next seg*/

  if(vas->segs == this)
    vas->segs = NULL;        /* seg list is gone*/
  else {
    this->prev->next = this->next;
    this->next->prev = prev;
  }

  if(vas->seglast == this)
    vas->seglast = vas->segs;

  //...

  kmem.free((char*) this);
}

void seg_t::unmap() {
  unmap(base, size);
}




