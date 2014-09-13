/*
 * proc.cpp
 *
 */

#include "types.h"
#include "kmem.h"
#include "hat.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "string.h"
#include "console.h"
#include "fs.h"
#include "mman.h"
#include "vas.h"

class ptable_t {
public:
  struct proc proc[NPROC];
};

ptable_t ptable;

static struct proc *initproc;

int nextpid = 1;

void forkret(void);

static void wakeup1(void *chan);

void pinit(void) {

}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void) {
  struct proc *p;
  char *sp;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  //cprintf("alloc pid:%d\n", p->pid);

  // Allocate kernel stack.
  if ((p->kstack = kmem.alloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*) sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*) sp = (uint) trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*) sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint) forkret;

  return p;
}

// Set up first user process.
void userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  //if ((p->pgdir = hat.setupkvm()) == 0)
    //panic("userinit: out of memory?");
  p->vas = vas_t::alloc();
  p->vas->p = p;

  //cprintf("proc:%s vas:0x%x pgdir:0x%x\n", p->name, p->vas, p->vas->hat->get_pgdir());

  p->vas->hat->inituvm(_binary_initcode_start, (int) _binary_initcode_size);
  //hat.inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei((char*) "/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
  uint sz;

  sz = proc->sz;
  if (n > 0) {
    if ((sz = proc->get_hat()->allocuvm(sz, sz + n)) == 0)
      return -1;
    //do_mmap(0, sz, n, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_FIXED|MAP_PRIVATE, 0);
  } else if (n < 0) {
    if ((sz = proc->get_hat()->deallocuvm(sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
  int i, pid;
  struct proc *np;

  // Allocate process.
  if ((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if ((np->vas = proc->vas->dup()) == 0) {
  //np->vas = vas_t::alloc();
  //if ((np->vas->hat = proc->get_hat()->copyuvm(proc->sz)) == 0) {
    kmem.free(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  //cprintf("nproc:%d alloc pgdir:0x%x\n", np->pid, np->get_hat()->get_pgdir());

  //cprintf("proc:%d page table:\n", proc->pid);
  //hat_t::dumppgdir(proc->vas->hat->get_pgdir(), 0, KERNBASE-1);

  //cprintf("np:%d page table:\n", np->pid);
  //hat_t::dumppgdir(np->vas->hat->get_pgdir(), 0, KERNBASE-1);

  np->vas->p = np;
  //np->vas->dumpseg();

  //cprintf("new proc:%d vas:0x%x pgdir:%x\r\n", np->pid, np->vas, np->vas->hat->get_pgdir());

  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (proc->ofile[i])
      np->ofile[i] = proc->ofile[i]->dup();
  np->cwd = proc->cwd->dup();

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
  struct proc *p;
  int fd;

  if (proc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (proc->ofile[fd]) {
      proc->ofile[fd]->close();
      proc->ofile[fd] = 0;
    }
  }

  proc->cwd->put();
  proc->cwd = 0;

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == proc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
  struct proc *p;
  int havekids, pid;

  for (;;) {
    // Scan through table looking for zombie children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != proc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE) {
        // Found one.
        pid = p->pid;
        kmem.free(p->kstack);
        p->kstack = 0;
        p->get_hat()->freevm();
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;

        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || proc->killed) {
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc); //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;

  for (;;) {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;

      //cprintf("prepare to pid:%d\n", p->pid);

      if (cpu->pid == p->pid) {
        cprintf("same proc, no swtch\n");
        continue;
      }

      //for debug
      //cprintf("scheduler from sys to pid:%d %s\n", proc->pid, proc->name);

      swtch(&cpu->scheduler, proc->context);

      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void sched(void) {
  int intena;

  //if(!holding(&ptable.lock))
  //panic("sched ptable.lock");
  //if(cpu->ncli != 1)
  //panic("sched locks");
  if (proc->state == RUNNING)
    panic("sched running");
  //if(readeflags()&FL_IF)
  //panic("sched interruptible");
  intena = cpu->intena;

  //cprintf("sched from pid:%d %s to sys\n", proc->pid, proc->name);

  swtch(&proc->context, cpu->scheduler);

  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  proc->state = RUNNABLE;
  sched();
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
  static int first = 1;

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    //initlog();
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan) {
  if (proc == 0)
    panic("sleep");

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  wakeup1(chan);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      return 0;
    }
  }

  return -1;
}

