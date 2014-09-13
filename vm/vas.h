/*
 * vas.h
 *
 */

#ifndef VAS_H_
#define VAS_H_

//#include "proc.h"
#include "seg.h"

class hat_t;

struct proc;

//Virtual address space for process
class vas_t {
public:
  static vas_t* alloc();

  void free();

  seg_t* segat(int addr);

  vas_t* dup();

  int addseg(seg_t* newseg);

  faultcode_t fault(int addr, uint size, enum fault_type type, enum seg_rw rw);

  int map(int addr, uint size);

  int unmap(int addr, uint size);

  void dumpseg();

public:
  void init() {
    segs = 0;
    seglast = 0;
    size = 0;
    rss = 0;
    //hat = 0;
  }

public:
  int gap(uint minlen, int *basep, uint *lenp, int flags, int addr);

public:
  uint lock:1;
  uint want:1;
  uint paglck:1;
  uint :13;

  seg_t* segs;
  seg_t* seglast;

  size_t size;
  size_t rss;

  struct proc* p;
  hat_t* hat;
};

/*
 * Flags for as_gap.
 */
#define AH_DIR      0x1 /* direction flag mask */
#define AH_LO       0x0 /* find lowest hole */
#define AH_HI       0x1 /* find highest hole */
#define AH_CONTAIN  0x2 /* hole must contain `addr' */

#endif /* VAS_H_ */
