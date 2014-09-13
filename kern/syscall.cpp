/*
 * syscall.cpp
 *
 */

#include "types.h"
#include "console.h"
#include "sysfile.h"
#include "sysproc.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "sysnum.h"
#include "syscall.h"


// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int fetchint(uint addr, int *ip) {
  if (addr >= proc->sz || addr + 4 > proc->sz)
    return -1;
  *ip = *(int*) (addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int fetchstr(uint addr, char **pp) {
  char *s, *ep;

  if (addr >= proc->sz)
    return -1;
  *pp = (char*) addr;
  ep = (char*) proc->sz;
  for (s = *pp; s < ep; s++)
    if (*s == 0)
      return s - *pp;
  return -1;
}

// Fetch the nth 32-bit system call argument.
int argint(int n, int *ip) {
  return fetchint(proc->tf->esp + 4 + 4 * n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size n bytes.  Check that the pointer
// lies within the process address space.
int argptr(int n, char **pp, int size) {
  int i;

  if (argint(n, &i) < 0)
    return -1;
  if ((uint) i >= proc->sz || (uint) i + size > proc->sz)
    return -1;
  *pp = (char*) i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int argstr(int n, char **pp) {
  int addr;
  if (argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

int (*syscalls[])(void) = {
  0,
  sys_fork,
  sys_exit,
  sys_wait,
  sys_pipe,
  sys_read,
  sys_kill,
  sys_exec,
  sys_fstat,
  sys_chdir,
  sys_dup,
  sys_getpid,
  sys_sbrk,
  sys_sleep,
  sys_uptime,
  sys_open,
  sys_write,
  sys_mknod,
  sys_unlink,
  sys_link,
  sys_mkdir,
  sys_close,
  sys_mmap,
  sys_munmap
};

void syscall(void) {
  uint num;

  num = proc->tf->eax;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    proc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n", proc->pid, proc->name, num);
    proc->tf->eax = -1;
  }
}

