/*
 * proc.h
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#ifndef PROC_H_
#define PROC_H_

#include "mmu.h"
#include "file.h"
#include "param.h"
#include "vas.h"
#include "hat.h"

class vas_t;
//class hat_t;

// Segments in proc->gdt.
#define NSEGS     7

// Per-CPU state
struct cpu {
  uchar id; // Local APIC ID; index into cpus[] below
  struct context *scheduler; // swtch() here to enter scheduler
  struct taskstate ts; // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS]; // x86 global descriptor table
  volatile uint started; // Has the CPU started?
  int ncli; // Depth of pushcli nesting.
  int intena; // Were interrupts enabled before pushcli?

  // Cpu-local storage variables; see below
  struct cpu *cpu;
  struct proc *proc; // The currently-running process.
  int pid;
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//extern struct cpu *cpu asm("%gs:0");       // &cpus[cpunum()]
extern struct cpu* cpu;
//extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc
extern struct proc* proc;

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate {
  UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
};

// Per-process state
typedef struct proc {

public:
  hat_t* get_hat() {
    if((vas) && (vas->hat))
      return vas->hat;
    return 0;
  }

public:
  uint sz; // Size of process memory (bytes)
  vas_t* vas;
  //pde_t* pgdir; // Page table
  char *kstack; // Bottom of kernel stack for this process
  enum procstate state; // Process state
  volatile int pid; // Process ID
  proc *parent; // Parent process
  struct trapframe *tf; // Trap frame for current syscall
  struct context *context; // swtch() here to run process
  void *chan; // If non-zero, sleeping on chan
  int killed; // If non-zero, have been killed
  file *ofile[NOFILE]; // Open files
  inode *cwd; // Current directory
  char name[16]; // Process name (debugging)
} proc_t;

extern "C" void swtch(struct context**, struct context*);
extern "C" void trapret(void);

struct proc* copyproc(struct proc*);
void exit(void);
int fork(void);
int growproc(int);
int kill(int);
void pinit(void);
void procdump(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void sleep(void*);
void userinit(void);
int wait(void);
void wakeup(void*);
void yield(void);

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

#endif /* PROC_H_ */

