/*
 * fs.cpp
 *
 *  Created on: Aug 6, 2013
 *      Author: qinghua
 */

#include "fs.h"
#include "types.h"
//#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "buf.h"
#include "fs.h"
#include "file.h"
#include "string.h"
#include "console.h"
#include "page_cache.h"
#include "ide.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
//static void itrunc(struct inode*);

class icache_t {
public:
  inode inodes[NINODE];
};

icache_t icache;

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bcache.read(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  bcache.relse(bp);
}

void
block::zero(int dev, int bno) {
  struct buf *bp;

  bp = bcache.read(dev, bno);
  memset(bp->data, 0, BSIZE);
  bcache.relse(bp);
}

uint
block::alloc(uint dev) {
  uint b, bi, m;
  struct buf *bp;
  struct superblock sb;

  bp = 0;
  readsb(dev, &sb);
  for(b = 0; b < sb.size; b += BPB){
    bp = bcache.read(dev, BBLOCK(b, sb.ninodes));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        bcache.relse(bp);
        zero(dev, b + bi);
        return b + bi;
      }
    }
    bcache.relse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
void
block::free(int dev, uint b)
{
  struct buf *bp;
  struct superblock sb;
  int bi, m;

  readsb(dev, &sb);
  bp = bcache.read(dev, BBLOCK(b, sb.ninodes));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;

  bcache.relse(bp);
}

inode*
inode::alloc(uint dev, short type) {
  uint inum;
  struct buf *bp;
  struct dinode *dip;
  struct superblock sb;

  readsb(dev, &sb);

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bcache.read(dev, IBLOCK(inum));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;

      bcache.relse(bp);
      return inode::get(dev, inum);
    }
    bcache.relse(bp);
  }
  panic("ialloc: no inodes");
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
inode*
inode::get(uint dev, uint inum)
{
  struct inode *ip, *empty;

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inodes[0]; ip < &icache.inodes[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->flags = 0;

  return ip;
}


// Copy a modified in-memory inode to disk.
void
inode::update()
{
  struct buf *bp;
  struct dinode *dip;

  bp = bcache.read(this->dev, IBLOCK(this->inum));
  dip = (struct dinode*)bp->data + this->inum%IPB;
  dip->type = this->type;
  dip->major = this->major;
  dip->minor = this->minor;
  dip->nlink = this->nlink;
  dip->size = this->size;
  memmove(dip->addrs, this->addrs, sizeof(this->addrs));

  bcache.relse(bp);
}

inode*
inode::dup() {
  this->ref++;
  return this;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
inode::lock()
{
  struct buf *bp;
  struct dinode *dip;

  if(this->ref < 1)
    panic("ilock");

  while(this->flags & I_BUSY)
    sleep(this);
  this->flags |= I_BUSY;

  if(!(this->flags & I_VALID)){
    bp = bcache.read(this->dev, IBLOCK(this->inum));
    dip = (struct dinode*)bp->data + this->inum%IPB;
    this->type = dip->type;
    this->major = dip->major;
    this->minor = dip->minor;
    this->nlink = dip->nlink;
    this->size = dip->size;
    memmove(this->addrs, dip->addrs, sizeof(this->addrs));
    bcache.relse(bp);
    this->flags |= I_VALID;
    if(this->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
inode::unlock()
{
  if(!(this->flags & I_BUSY) || this->ref < 1)
    panic("iunlock");

  this->flags &= ~I_BUSY;
  wakeup(this);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
void
inode::put()
{
  if(this->ref == 1 && (this->flags & I_VALID) && this->nlink == 0){
    // inode has no links: truncate and free inode.
    if(this->flags & I_BUSY)
      panic("iput busy");
    this->flags |= I_BUSY;
    trunc();
    this->type = 0;
    update();
    this->flags = 0;
    wakeup((void*)this);
  }

  this->ref--;
}

// Common idiom: unlock, then put.
void
inode::unlockput()
{
  unlock();
  put();
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
uint inode::bmap(uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = this->addrs[bn]) == 0)
      this->addrs[bn] = addr = block::alloc(this->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = this->addrs[NDIRECT]) == 0)
      this->addrs[NDIRECT] = addr = block::alloc(this->dev);
    bp = bcache.read(this->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = block::alloc(this->dev);

    }
    bcache.relse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
void
inode::trunc()
{
  uint i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(this->addrs[i]){
      block::free(this->dev, this->addrs[i]);
      this->addrs[i] = 0;
    }
  }

  if(this->addrs[NDIRECT]){
    bp = bcache.read(this->dev, this->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        block::free(this->dev, a[j]);
    }
    bcache.relse(bp);
    block::free(this->dev, this->addrs[NDIRECT]);
    this->addrs[NDIRECT] = 0;
  }

  this->size = 0;
  update();
}

int
inode::rw_page(page_t* page, int  rw) {

  unsigned long iblock, lblock;
  unsigned int blocksize, blocks;
  int nr,i;

  blocksize = 512;
  //page->create_empty_buffers(page, dev);
  //buf_t* head = page->buffers;

  blocks = PGSIZE >> BLKBITS;
  iblock = page->offset << (PGSHIFT - BLKBITS);
  lblock = (size + blocksize - 1) >> BLKBITS;  //last block
  //buf_t* b = head;
  nr = 0;
  i = 0;

  do {
    buf_t* b = bcache.get(0, 0);

    uint sector = -1;
    if(iblock <= lblock)
      sector = bmap(iblock);
    else
      break;
    b->sector = sector;
    b->dev = dev;

    if(rw == FS_WRITE)
      b->flags |= B_DIRTY;
    ide.rw(b);

    int cpn = BSIZE;
    if(iblock == lblock) {
      cpn = BSIZE - this->size % BSIZE;
    }
    if(rw == FS_READ)
      memmove(page->vaddr + i*BSIZE, b->data, cpn);

    bcache.relse(b);
  } while(i++, iblock++, i < blocks);

  return 0;
}

int
inode::write_page(page_t* page, char* src, unsigned from, unsigned to) {
  unsigned long iblock, lblock;
  unsigned int blocksize, blocks;
  int nr,i;

  blocksize = 512;
  //page->create_empty_buffers(page, dev);
  //buf_t* head = page->buffers;

  blocks = PGSIZE >> BLKBITS;
  iblock = page->offset << (PGSHIFT - BLKBITS);
  lblock = (size + blocksize - 1) >> BLKBITS;  //last block
  //buf_t* b = head;
  nr = 0;
  i = 0;

  do {
    buf_t* b = bcache.get(0, 0);
    uint sector = bmap(iblock);
    b->sector = sector;
    b->dev = dev;

    //b->flags |= B_DIRTY;
    ide.rw(b);

    //memmove(page->vaddr + i*512, b->data, 512);

    bcache.relse(b);
  } while(i++, iblock++, i < blocks);

  memmove(page->vaddr + from, src, (to - from));

  return to - from;
}

int
inode::read_page(page_t* page) {
  uint off = page->offset << PGSHIFT;
  return read((char*)page->vaddr, off, PGSIZE);
}

// Read data from inode. old stype
int
inode::read(char *dst, uint off, uint n)
{
  if(this->type == T_DEV){
    if(this->major < 0 || this->major >= NDEV || !devsw[this->major].read)
      return -1;
    return devsw[this->major].read(this, dst, n);
  }

  uint tot, m;
  struct buf *bp;

  if(off > this->size || off + n < off)
    return -1;
  if(off + n > this->size)
    n = this->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bcache.read(this->dev, bmap(off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    bcache.relse(bp);
  }
  return n;
}

//Read data from page cache.
int
inode::read2(char *dst, uint off, uint n) {

  if(this->type == T_DEV){
    if(this->major < 0 || this->major >= NDEV || !devsw[this->major].read)
      return -1;
    return devsw[this->major].read(this, dst, n);
  } else if (this->type == T_DIR) {
    return this->read2(dst, off, n);
  }

  unsigned long index, offset;
  index = off >> 12;        //page index inside file
  offset = off & ~PAGEMASK; //offset inside page
  unsigned int total = 0;   //total bytes have read

  for(;;) {
    page_t* page;
    unsigned long end_index;    //last page index
    unsigned long bytes;        //byte num that has to be read within the page

    end_index = this->size >> 12;

    if(index > end_index)
      break;
    bytes = PGSIZE;
    if(index == end_index) {
      bytes = this->size & (PGSIZE -1);
      if(bytes <= offset)
        break;
    }

    bytes = bytes - offset;

    page = page_cache.find_page(this, index);
    if(!page) {
      page = page_cache.get_page(this, index);
      rw_page(page, FS_READ);
    }

    //copy page to user
    if(bytes > n)
      bytes = n;
    memmove(dst, page->vaddr+offset, bytes);
    n -= bytes;
    total += bytes;
    dst += bytes;
    offset += bytes;
    index += offset >> PGSHIFT;
    offset &= (PGSIZE -1);

    if(n)
      continue;
    break;
  }

  return total;
}

// Write data to inode.
int
inode::write(char *src, uint off, uint n) {

  uint tot, m;
  struct buf *bp;

  if(this->type == T_DEV){
    if(this->major < 0 || this->major >= NDEV || !devsw[this->major].write)
      return -1;
    return devsw[this->major].write(this, src, n);
  }

  if(off > this->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    bp = bcache.read(this->dev, bmap(off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    bcache.relse(bp);
  }

  if(n > 0 && off > this->size){
    this->size = off;
    update();
  }

  return n;
}

int
inode::write2(char *src, uint pos, uint count) {

  if(this->type == T_DEV){
    if(this->major < 0 || this->major >= NDEV || !devsw[this->major].write)
      return -1;
    return devsw[this->major].write(this, src, count);
  }

  if(this->type == T_DIR)
    return write2(src, pos, count);

  unsigned bytes;
  int status = 0;
  int written = 0;

  do {
    unsigned long index, offset;

    offset = pos & (PGSIZE -1); //offset within page
    index = pos >> PGSHIFT;
    bytes = PGSIZE - offset;

    if(bytes > count)
      bytes = count;

    page_t* page = page_cache.get_page(this, index);
    if(!page) {
      cprintf("can't get page in page cache");
      return -1;
    }

    status = write_page(page, src, offset, offset + bytes);
    if(!status)
      status = bytes;

    if(status >= 0) {
      written += status;
      count -= status;
      offset += status;
      src += status;
    }
  } while(count > 0);

  if(count > 0 && pos > this->size){
    this->size = pos;
    update();
  }

  return written ? written : status;
}

// Copy stat information from inode.
void
inode::stat(struct stat *st)
{
  st->dev = dev;
  st->ino = inum;
  st->type = type;
  st->nlink = nlink;
  st->size = size;
}

// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(dp->read((char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return inode::get(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(inode *dp, char *name, uint inum)
{
  uint off;
  struct dirent de;
  inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    ip->put();
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(dp->read((char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(dp->write((char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  inode *ip, *next;

  if(*path == '/')
    ip = ip->get(ROOTDEV, ROOTINO);
  else
    ip = proc->cwd->dup();

  while((path = skipelem(path, name)) != 0){
    ip->lock();
    if(ip->type != T_DIR){
      ip->unlockput();
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      ip->unlock();
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      ip->unlockput();
      return 0;
    }
    ip->unlockput();
    ip = next;
  }
  if(nameiparent){
    ip->put();
    return 0;
  }
  return ip;
}

inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}

