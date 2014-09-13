/*
 * file.cpp
 */

//
// File descriptors
//

#include "types.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "console.h"
#include "pipe.h"

struct devsw devsw[NDEV];

class ftable_t {
public:
  file files[NFILE];
};

ftable_t ftable;

void
fileinit(void)
{

}

// Allocate a file structure.
file*
file::alloc(void)
{
  file *f;

  for(f = ftable.files; f < ftable.files + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      return f;
    }
  }

  return 0;
}

// Increment ref count for file f.
file*
file::dup()
{
  if(ref < 1)
    panic("filedup");
  ref++;
  return this;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
file::close()
{
  file ff;

  if(ref < 1)
    panic("fileclose");
  if(--ref > 0){
    return;
  }
  ff = *this;
  ref = 0;
  type = FD_NONE;

  if(ff.type == FD_PIPE)
    ff.pipe->close(ff.writable);
  else if(ff.type == FD_INODE){
    ff.ip->put();
  }
}

// Get metadata about file f.
int
file::stat(struct stat *st)
{
  if(type == FD_INODE){
    ip->lock();
    ip->stat(st);
    ip->unlock();
    return 0;
  }
  return -1;
}

// Read from file f.
int
file::read(char *addr, int n)
{
  int r;

  if(readable == 0)
    return -1;

  if(type == FD_PIPE)
    return this->pipe->read(addr, n);

  if(type == FD_INODE){
    ip->lock();
    if((r = ip->read(addr, off, n)) > 0)
      off += r;
    ip->unlock();
    return r;
  }
  panic("fileread");
}

// Write to file f.
int
file::write(char *addr, int n)
{
  int r;

  if(writable == 0)
    return -1;

  if(type == FD_PIPE)
    return this->pipe->write(addr, n);

  if(type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      ip->lock();
      if ((r = this->ip->write(addr + i, off, n1)) > 0)
        off += r;
      ip->unlock();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}




