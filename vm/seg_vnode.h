/*
 * seg_vn.h
 *
 *  Created on: Oct 9, 2013
 *      Author: qinghua
 */

#ifndef SEG_VN_H_
#define SEG_VN_H_

#include "seg.h"
#include "mmu.h"

class page_t;
class inode;

class seg_vnode_t: public seg_t {

public:

  seg_vnode_t() {
    vp = 0;
    offset = 0;
    prot = 0;
  }

  void* operator new(uint size);

  void operator delete(void* );

  int dup(seg_t* newseg);

  int unmap(int addr, uint len);

  faultcode_t fault(int addr, uint len, enum fault_type type, enum seg_rw rw);

  //virtual ~seg_vnode_t() {  }

private:

  page_t* file_fault(int addr);

  int no_page(int addr, enum seg_rw rw, pte_t* page_table);

  int wp_page(int addr, pte_t* pte);

  int anon_page(int addr, enum seg_rw rw, pte_t* page_table);

public:
  inode* vp;          /* vnode that segment mapping is to */
  uint offset;        /* starting offset of vnode for mapping */
  //uchar type;         /* type of sharing done */
  uchar prot;         /* current segment prot */
};


#endif /* SEG_VN_H_ */
