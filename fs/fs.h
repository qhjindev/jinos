
#ifndef FS_H_
#define FS_H_

#include "param.h"
#include "types.h"
//#include "page_cache.h"

class page_t;

// On-disk file system format.
// Both the kernel and user programs use this header file.

// Block 0 is unused.
// Block 1 is super block.
// Blocks 2 through sb.ninodes/IPB hold inodes.
// Then free bitmap blocks holding sb.size bits.
// Then sb.nblocks data blocks.
// Then sb.nlog log blocks.

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size
#define BLKBITS 9

#define FS_READ 0
#define FS_WRITE 1

// File system super block
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

class block {
public:
  static uint alloc(uint dev);
  static void zero(int dev, int bno);
  static void free(int dev, uint b);

};

class inode {
public:
  //static void init() {}
  static inode* get(uint dev, uint inum);
  static inode* alloc(uint dev, short type);
  uint bmap(uint bn);

  void update();
  inode* dup();
  void lock();
  void unlock();
  void put();
  void unlockput();
  void trunc();

  int rw_page(page_t* page, int rw);
  int read_page(page_t* page);
  int write_page(page_t* page, char* src, unsigned from, unsigned to);

  int read(char* dst, uint off, uint n);
  int read2(char* dst, uint off, uint n);

  int write(char* src, uint off, uint n);
  int write2(char* src, uint off, uint n);

  void stat(struct stat* st);

public:
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];

  page_t* pages;
};


#define I_BUSY 0x1
#define I_VALID 0x2

int             dirlink(inode*, char*, uint);
struct inode*   dirlookup(inode*, char*, uint*);

int      namecmp(const char*, const char*);
inode*   namei(char*);
inode*   nameiparent(char*, char*);


#endif /* FS_H_ */
