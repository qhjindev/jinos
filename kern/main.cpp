/*
 * main.cpp
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#include "types.h"
#include "console.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "hat.h"
#include "kmem.h"
#include "pic.h"
#include "trap.h"
#include "buf.h"
#include "ide.h"
#include "timer.h"
#include "kbd.h"

struct cpu* cpu;
struct proc* proc;

extern char end[]; // first address after kernel loaded from ELF file
extern page_t* pages;
extern page_t* epages;

// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  char* start_addr = kmem.init(); // phys page allocator
  //kmem.init_range(P2V(mapend), P2V(4*1024*1024));
  kmem.init_range((char*)(epages), P2V(4*1024*1024));
  kvmalloc();      // kernel page table
  cpu = (struct cpu*)kmem.alloc();
  cpu->proc = 0;
  cpu->pid = -1;
  proc = cpu->proc;

  seginit();       // set up segments
  cprintf("\nWelcome to jinos\n\n");
  kbdinit();
  pic.init();       // interrupt controller
  consoleinit();   // I/O devices & their interrupts
  pinit();         // process table
  trapvectorinit();        // trap vectors
  bcache.init();         // buffer cache
  page_cache.init();
  //fileinit();      // file table
  //iinit();         // inode cache
  ide.init();       // disk
  timerinit();   // uniprocessor timer
  kmem.init_range(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process

  //cprintf("cpu%d: starting\n", cpu->id);
  idtinit();       // load idt register
  xchg(&cpu->started, 1); // tell startothers() we're up
  scheduler();     // start running processes
}

