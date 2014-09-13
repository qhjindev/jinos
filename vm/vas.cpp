/*
 * vas.cpp
 *
 *  Created on: Oct 9, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "param.h"
#include "errno.h"
#include "hat.h"
#include "hat.h"
#include "vas.h"
#include "memlayout.h"
#include "kmem.h"
#include "seg_vnode.h"
#include "console.h"
#include "x86.h"
#include "proc.h"

int
valid_usr_range(int addr, size_t len) {
  register int eaddr = addr + len;

  if (eaddr <= addr || addr < (int)0 || eaddr >= (int)KERNBASE)
    return(0);
  return(1);
}

int valid_va_range(int *basep, uint *lenp, uint minlen, int dir) {
  register uint hi, lo;

  lo = *basep;
  hi = lo + *lenp;
  if (hi < lo) /* overflow */
    return (0);
  if (hi - lo < minlen)
    return (0);
  return (1);
}

vas_t* vas_t::alloc() {
  vas_t* vas = (vas_t*)kmem.alloc();
  vas->init();

  vas->hat = hat_t::alloc();

  return vas;
}

void vas_t::free() {
  //hat_t::free(this);
  //while (segs != NULL)
    //segs->unmap();
  //kmem.free((char*) this);
  segs = 0;
  seglast = 0;
  flush_tlb();
}

/* Find a segment containing addr. */
seg_t* vas_t::segat(int addr) {
  seg_t *seg, *sseg;

  if (seglast == NULL)
    seglast = segs;

  sseg = seg = seglast;
  if (seg != NULL) {
    do {
      if (seg->base <= addr && addr < (seg->base + seg->size)) {
        seglast = seg;
        return seg;
      }

      seg = seg->next;
    } while (seg != sseg);
  }

  return NULL;
}

vas_t* vas_t::dup() {

  //cprintf("start dup vas\n");

  vas_t* newas;
  seg_t *seg, *sseg, *newseg;

  newas = alloc();
  sseg = seg = segs;

  if (seg != NULL) {
    do {
      newseg = seg_t::alloc(newas, seg->base, seg->size);
      if (newseg == NULL) {
        newas->free();
        return NULL;
      }

      //cprintf("dup seg start:0x%x size:0x%x\n", seg->base, seg->size);

      if (newseg->dup(seg) != 0) {
        newseg->free();
        newas->free();
        return NULL;
      }

      newas->size = (newas->size + seg->size);
      seg = seg->next;
    } while (seg != sseg);
  }

  return newas;
}

void vas_t::dumpseg() {
  cprintf("start dump proc:%d segs\n", this->p->pid);

  seg_t *seg, *sseg;

  if (seglast == NULL)
    seglast = segs;

  sseg = seg = seglast;
  if (seg != NULL) {
    do {
      cprintf("seg base:0x%x size:0x%x\r\n", seg->base, seg->size);
      seg = seg->next;
    } while (seg != sseg);
  }
}

int vas_t::addseg(seg_t* newseg) {
  seg_t* seg;
  int base;
  int eaddr;

  seg = segs;
  if (seg == NULL) {
    newseg->prev = (newseg);
    newseg->next = (newseg);
    segs = newseg;
  } else {
    /*
     * Figure out where to add the segment to keep list sorted
     */
    base = newseg->base;
    eaddr = base + newseg->size;
    do {
      if (base < seg->base) {
        if (eaddr > seg->base)
          return (-1);
        break;
      }
      if (base < seg->base + seg->size)
        return (-1);
      seg = seg->next;
    } while (seg != segs);

    newseg->next = (seg);
    newseg->prev = seg->prev;
    seg->prev = newseg;
    newseg->prev->next = newseg;

    if (base < segs->base)
      segs = newseg; /* newseg is at front */
  }

  return (0);
}

faultcode_t vas_t::fault(int addr, uint size, enum fault_type type, enum seg_rw rw) {
  seg_t* seg;
  int raddr;
  uint rsize;
  uint ssize;
  faultcode_t res = 0;
  int addrsav;
  seg_t* segsav;

  raddr = (int) ((uint) addr & PAGEMASK);
  rsize = (((uint) (addr + size) + PAGEOFFSET) & PAGEMASK) - (uint) raddr;

  seg = segat(raddr);
  if (seg == NULL) {
    cprintf("err find seg at addr:0x%x\n", raddr);
    panic("panic for not find seg\n");
    return -1;
  }
  addrsav = raddr;
  segsav = seg;

  do {
    if (raddr >= seg->base + seg->size) {
      seg = seg->next; /* goto next seg */
      if (raddr != seg->base) {
        res = -1;
        break;
      }
    }
    if (raddr + rsize > seg->base + seg->size)
      ssize = seg->base + seg->size - raddr;
    else
      ssize = rsize;

    seg_vnode_t* segv = (seg_vnode_t*)seg;
    //seg_vnode_t* segv = (seg_vnode_t*)kmem.alloc();
    res = segv->fault(raddr, ssize, type, rw);
    if (res != 0)
      break;

    raddr += ssize;
    rsize -= ssize;
  } while (rsize != 0);

  return res;
}

int vas_t::unmap(int addr, uint size) {
  register seg_t *seg, *seg_next;
  register int raddr, eaddr;
  register uint ssize;
  int obase;

  raddr = (int) ((uint) addr & PAGEMASK);
  eaddr = (int) (((uint) (addr + size) + PAGEOFFSET) & PAGEMASK);

  seg_next = segs;
  if (seg_next != NULL) {
    do {
      /*
       * Save next segment pointer since seg can be
       * destroyed during the segment unmap operation.
       * We also have to save the old base.
       */
      seg = seg_next;
      seg_next = seg->next;
      obase = seg->base;

      if (raddr >= seg->base + seg->size)
        continue; /* not there yet */

      if (eaddr <= seg->base)
        break; /* all done */

      if (raddr < seg->base)
        raddr = seg->base; /* skip to seg start */

      if (eaddr > (seg->base + seg->size))
        ssize = seg->base + seg->size - raddr;
      else
        ssize = eaddr - raddr;

      if (seg->unmap(raddr, ssize) != 0)
        return (-1);

      this->size -= ssize;
      raddr += ssize;

      /*
       * Check to see if we have looked at all the segs.
       *
       * We check this->segs because the unmaps above could
       * have unmapped the last segment.
       */
    } while (this->segs != NULL && obase < seg_next->base);
  }

  return (0);
}

int vas_t::map(int addr, uint size) {
  register struct seg_t *seg;
  register int raddr; /* rounded down addr */
  register uint rsize; /* rounded up size */
  int error;

  raddr = (int) ((uint) addr & PAGEMASK);
  rsize = (((uint) (addr + size) + PAGEOFFSET) & PAGEMASK) - (uint) raddr;

  seg = seg_t::alloc(this, addr, size);
  if (seg == NULL)
    return (ENOMEM);

  /*
   * Remember that this was the most recently touched segment.
   * If the create routine merges this segment into an existing
   * segment, seg_free will adjust the a_seglast hint.
   */
  seglast = seg;
  /*
   error = (*crfp)(seg, argsp);
   if (error != 0) {
   seg_free(seg);
   } else {
   as->a_size += rsize;
   }
   */
  return (error);
}

int vas_t::gap(uint minlen, int *basep, uint *lenp, int flags, int addr) {
  register uint lobound, hibound;
  register uint lo, hi;
  register seg_t *seg, *sseg;

  lobound = *basep;
  hibound = lobound + *lenp;
  if (lobound > hibound) /* overflow */
    return (-1);

  sseg = seg = this->segs;
  if (seg == NULL) {
    if (valid_va_range(basep, lenp, minlen, flags & AH_DIR))
      return (0);
    else
      return (-1);
  }

  if ((flags & AH_DIR) == AH_LO) { /* search from lo to hi */
    lo = lobound;
    do {
      hi = seg->base;
      if (hi > lobound && hi > lo) {
        *basep = MAX(lo, lobound);
        *lenp = MIN(hi, hibound) - *basep;
        if (valid_va_range(basep, lenp, minlen, AH_LO)
            && ((flags & AH_CONTAIN) == 0 || (*basep <= addr && *basep + *lenp > addr)))
          return (0);
      }
      lo = seg->base + seg->size;
    } while (lo < hibound && (seg = seg->next) != sseg);

    if (hi < lo)
      hi = hibound;

    /* check against upper bound */
    if (lo < hibound) {
      *basep = MAX(lo, lobound);
      *lenp = MIN(hi, hibound) - *basep;
      if (valid_va_range(basep, lenp, minlen, AH_LO)
          && ((flags & AH_CONTAIN) == 0 || (*basep <= addr && *basep + *lenp > addr)))
        return (0);
    }
  } else { /* search from hi to lo */
    seg = seg->prev;
    hi = hibound;
    do {
      lo = seg->base + seg->size;
      if (lo < hibound && hi > lo) {
        *basep = MAX(lo, lobound);
        *lenp = MIN(hi, hibound) - *basep;
        if (valid_va_range(basep, lenp, minlen, AH_HI)
            && ((flags & AH_CONTAIN) == 0 || (*basep <= addr && *basep + *lenp > addr)))
          return (0);
      }
      hi = seg->base;
    } while (hi > lobound && (seg = seg->prev) != sseg);

    if (lo > hi)
      lo = lobound;

    /* check against lower bound */
    if (hi > lobound) {
      *basep = MAX(lo, lobound);
      *lenp = MIN(hi, hibound) - *basep;
      if (valid_va_range(basep, lenp, minlen, AH_HI)
          && ((flags & AH_CONTAIN) == 0 || (*basep <= addr && *basep + *lenp > addr)))
        return (0);
    }
  }
  return (-1);
}
