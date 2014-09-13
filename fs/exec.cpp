/*
 * exec.cpp
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "elf.h"
#include "hat.h"
#include "fs.h"
#include "string.h"
#include "console.h"
#include "mman.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  inode *ip;
  struct proghdr ph;
  //pde_t *pgdir, *oldpgdir;
  hat_t *hat, *oldhat;
  int osz = 0;
  cprintf("start exec:%s proc:%d\n", path, proc->pid);

  proc_t* cur = proc;

  if((ip = namei(path)) == 0)
    return -1;
  ip->lock();
  //pgdir = 0;

  //proc_t* cur = proc;
  //cprintf("exec in proc:%s\n", cur->name);

  // Check ELF header
  if(ip->read((char*)&elf, 0, sizeof(elf)) < (int)sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  hat = hat_t::alloc();
  //cprintf("proc:%d alloc pgdir:0x%x\n", cur->pid, hat->get_pgdir());

  proc->vas->free();
  safestrcpy(proc->name, path, sizeof(path));

  //if((pgdir = setupkvm()) == 0)
    //goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(ip->read((char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;

    if((sz = hat->allocuvm(sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(hat->loaduvm((char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;

    //cprintf("start map addr:0x%x len:0x%x to proc:%d\n", ph.vaddr, ph.memsz, proc->pid);

    do_mmap(ip, ph.vaddr, ph.memsz, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_FIXED|MAP_PRIVATE, ph.off);
  }
  ip->unlockput();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);

  osz = sz;
  if((sz = hat->allocuvm(sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  do_mmap(0, osz, 2*PGSIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_FIXED|MAP_PRIVATE, 0);
  hat->clearpteu((char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    //memcpy((void*)sp, (const void*)argv[argc], strlen(argv[argc])+1);
    if(hat->copyout(sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(hat->copyout(sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=(char*)path; *s; s++)
    if(*s == '/') last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  //oldpgdir = proc->pgdir;
  //proc->pgdir = pgdir;
  oldhat = proc->get_hat();
  proc->vas->hat = hat;

  proc->sz = sz;
  proc->tf->eip = elf.entry;  // main
  proc->tf->esp = sp;
  switchuvm(proc);
  //hat.freevm(oldpgdir);
  oldhat->freevm();
  return 0;

bad:
//  if(pgdir)
//    hat.freevm(pgdir);
  if(hat)
    hat->freevm();
  if(ip)
    ip->unlockput();
  return -1;
}



