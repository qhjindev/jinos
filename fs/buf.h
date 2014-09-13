#ifndef BUF_H
#define BUF_H

#include "param.h"
#include "types.h"
#include "string.h"

#define B_BUSY  0x1  // buffer is locked by some process
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

typedef struct buf {
  int flags;
  uint dev;
  uint sector;
  struct buf *prev; // LRU cache list
  struct buf *next;
  struct buf *qnext; // disk queue
  //page_t *this_page;
  uchar data[512];
  void init() {
    memset(this, 0, sizeof(buf));
  }
} buf_t;

class bcache_t {
public:
  void init();
  struct buf* get(uint dev, uint sector);
  struct buf* read(uint dev, uint sector);
  void write(struct buf* b);
  void relse(struct buf* b);

private:
  struct buf buf[NBUF];
  struct buf head;
};

extern bcache_t bcache;

#endif //BUF_H
