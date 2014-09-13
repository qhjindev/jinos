/*
 * seg.h
 *
 *  Created on: Oct 7, 2013
 *      Author: qinghua
 */

#ifndef SEG_H_
#define SEG_H_

typedef int faultcode_t;

enum fault_type {
  F_INVAL, /* invalid page*/
  F_PROT /* protection fault*/
};

enum seg_rw {
  S_OTHER, /* unknown or not touched */
  S_READ, /* read access attempted */
  S_WRITE, /* write access attempted */
  S_EXEC /* execution access attempted */
};

class vas_t;

class seg_t {
public:
  seg_t() {
    base = 0;
    size = 0;
    vas = 0;
    type = 0;

    next = 0;
    prev = 0;
  }

  static seg_t* alloc(vas_t* vas, int base, uint size);

  int attach(vas_t* vas, int base, uint size);

  void free();

  void unmap();

  virtual ~seg_t() {  }

  void operator delete(void* );

public:

  virtual int dup(seg_t* newseg);

  virtual int unmap(int addr, uint len);

  virtual faultcode_t fault(int addr, uint len, enum fault_type type, enum seg_rw rw);

public:
  int base;
  uint size;
  vas_t* vas;
  uchar type;

  seg_t* next;
  seg_t* prev;
};

#endif /* SEG_H_ */
