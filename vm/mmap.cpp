/*
 * mmap.cpp
 *
 *  Created on: Oct 17, 2013
 *      Author: qinghua
 */

/* mmap args
 unsigned long addr, unsigned long len, unsigned long prot,
 unsigned long flags, unsigned long fd, unsigned long pgoff
 */

#include "types.h"
#include "memlayout.h"
#include "param.h"
#include "mmu.h"
#include "vas.h"
#include "mman.h"
#include "errno.h"
#include "types.h"
#include "proc.h"
#include "seg_vnode.h"
#include "console.h"

void map_addr(uint *addrp, uint len, off_t off, int align) {
  vas_t *vas = proc->vas;
  int base;
  uint slen;

  base = (int) 0;
  slen = (int) KERNBASE - base;
  len = (len + PAGEOFFSET) & PAGEMASK;

  len += 2 * PGSIZE;

  /*
   * Look for a large enough hole starting above UVSHM.
   * After finding it, use the lower part.
   */
  if (vas->gap(len, &base, &slen, AH_LO, (int) NULL) == 0)
    //*addrp = ((uint) base + PGSIZE);
    *addrp = base;
  else
    *addrp = NULL; /* no more virtual space */
}

int do_mmap(inode* ip, uint addr, int len, int prot, int flags, int off) {
  vas_t* vas = proc->vas;
  seg_t *seg, *prev;
  uint *addrp;

  //cprintf("start map addr:0x%x len:0x%x to proc:%d\n", addr, len, proc->pid);

  if ((flags & MAP_FIXED) == 0) {
    map_addr(addrp, len, off, 1);
    int tmp = (int)(*addrp);
    //if (tmp == (int)0)
      //return (ENOMEM);
  } else
    addrp = &addr;

  seg_vnode_t* nseg = (seg_vnode_t*) seg_t::alloc(vas, *addrp, len);
  nseg->vp = ip;
  nseg->offset = off & PAGEMASK;
  nseg->type = flags | MAP_TYPE;
  nseg->prot = prot;

  vas->seglast = nseg;

  return (int)(*addrp);
}

