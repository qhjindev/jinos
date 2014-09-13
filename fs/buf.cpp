#include "buf.h"
#include "fs.h"
#include "types.h"
#include "ide.h"
#include "proc.h"
#include "console.h"

bcache_t bcache;

void bcache_t::init() {
  struct buf* b;
  head.prev = &head;
  head.next = &head;
  for (b = buf; b < buf + NBUF; b++) {
    b->init();
    b->next = head.next;
    b->prev = &head;
    b->dev = -1;
    head.next->prev = b;
    head.next = b;
  }
}

struct buf*
bcache_t::get(uint dev, uint sector) {
  struct buf* b;
  loop:
  // Is the sector already cached?
  for (b = head.next; b != &head; b = b->next) {
    if (b->dev == dev && b->sector == sector) {
      if (!(b->flags & B_BUSY)) {
        b->flags |= B_BUSY;
        return b;
      }
      sleep(b);
      goto loop;
    }
  }

  // Not cached; recycle some non-busy and clean buffer.
  for (b = head.prev; b != &head; b = b->prev) {
    if ((b->flags & B_BUSY) == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->sector = sector;
      b->flags = B_BUSY;

      return b;
    }
  }
  panic("bget: no buffers");
}

// Release a B_BUSY buffer.
// Move to the head of the MRU list.
void bcache_t::relse(struct buf *b) {
  if ((b->flags & B_BUSY) == 0)
    panic("brelse");

  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = head.next;
  b->prev = &head;
  head.next->prev = b;
  head.next = b;

  b->flags &= ~B_BUSY;
  wakeup(b);
}

// Return a B_BUSY buf with the contents of the indicated disk sector.
struct buf*
bcache_t::read(uint dev, uint sector) {
  struct buf *b;

  b = get(dev, sector);
  if (!(b->flags & B_VALID))
    ide.rw(b);
  return b;
}

// Write b's contents to disk.  Must be B_BUSY.
void bcache_t::write(struct buf *b) {
  if ((b->flags & B_BUSY) == 0)
    panic("bwrite");
  b->flags |= B_DIRTY;
  ide.rw(b);
}











