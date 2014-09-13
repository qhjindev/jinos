/*
 * sysfile.cpp
 *
 */

//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "sysfile.h"
#include "syscall.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "hat.h"
#include "console.h"
#include "string.h"
#include "pipe.h"
#include "errno.h"
#include "mman.h"
#include "vas.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=proc->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;

  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd] == 0){
      proc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_mmap(void) {

  struct file *f;
  int addr, len,  prot, flags, fd, pgoff;

  argint(0, &addr);
  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argfd(4, 0, &f);
  argint(5, &pgoff);

  return do_mmap(f->ip, addr, len, prot, flags, pgoff);
}


int
sys_munmap(void) {
  int addr, len;

  argint(0, &addr);
  argint(1, &len);

  proc_t* p = proc;
  if(((int)addr & PAGEOFFSET) != 0) {
    return (EINVAL);
  }

  if(valid_usr_range((int)addr, len) != 0) {
    return (EINVAL);
  }

  if(proc->vas->unmap((int)addr, len) != 0)
    return EINVAL;

  return 0;
}

int
sys_dup(void)
{
  file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  f->dup();
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return f->read(p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return f->write(p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  proc->ofile[fd] = 0;
  f->close();
  return 0;
}

int
sys_fstat(void)
{
  file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (char**)&st, sizeof(*st)) < 0)
    return -1;
  return f->stat(st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *newp, *old;
  inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &newp) < 0)
    return -1;
  if((ip = namei(old)) == 0)
    return -1;

  //begin_trans();

  ip->lock();
  if(ip->type == T_DIR){
    ip->unlockput();
    //commit_trans();
    return -1;
  }

  ip->nlink++;
  ip->update();
  ip->unlock();

  if((dp = nameiparent(newp, name)) == 0)
    goto bad;
  dp->lock();
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    dp->unlockput();
    goto bad;
  }
  dp->unlockput();
  ip->put();

  //commit_trans();

  return 0;

bad:
  ip->lock();
  ip->nlink--;
  ip->update();
  ip->unlockput();
  //commit_trans();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  uint off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(dp->read((char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;
  if((dp = nameiparent(path, name)) == 0)
    return -1;

  //begin_trans();

  dp->lock();

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ip->lock();

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    ip->unlockput();
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(dp->write((char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    dp->update();
  }
  dp->unlockput();

  ip->nlink--;
  ip->update();
  ip->unlockput();

  //commit_trans();

  return 0;

bad:
  dp->unlockput();
  //commit_trans();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  uint off;
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  dp->lock();

  if((ip = dirlookup(dp, name, &off)) != 0){
    dp->unlockput();
    ip->lock();
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    ip->unlockput();
    return 0;
  }

  if((ip = inode::alloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ip->lock();
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->update();

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    dp->update();
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, (char*)".", ip->inum) < 0 || dirlink(ip, (char*)"..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  dp->unlockput();

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;
  if(omode & O_CREATE){
    //begin_trans();
    ip = create(path, T_FILE, 0, 0);
    //commit_trans();
    if(ip == 0)
      return -1;
  } else {
    if((ip = namei(path)) == 0)
      return -1;
    ip->lock();
    if(ip->type == T_DIR && omode != O_RDONLY){
      ip->unlockput();
      return -1;
    }
  }

  if((f = file::alloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      f->close();
    ip->unlockput();
    return -1;
  }
  ip->unlock();

  f->type = file::FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  //begin_trans();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    //commit_trans();
    return -1;
  }
  ip->unlockput();
  //commit_trans();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int len;
  int major, minor;

  //begin_trans();
  if((len=argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    //commit_trans();
    return -1;
  }
  ip->unlockput();
  //commit_trans();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;

  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0)
    return -1;
  ip->lock();
  if(ip->type != T_DIR){
    ip->unlockput();
    return -1;
  }
  ip->unlock();
  proc->cwd->put();
  proc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  uint i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (char**)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipe::alloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      proc->ofile[fd0] = 0;
    rf->close();
    wf->close();
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

