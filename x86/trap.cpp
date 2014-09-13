/*
 * trap.cpp
 *
 */

#include "x86.h"
#include "traps.h"
#include "mmu.h"
#include "kbd.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
#include "ide.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
uint ticks;

void
trapvectorinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

int
kern_page_fault(uint errcode, int faultadr) {
  cprintf("kernel pgflt addr:0x%x err:0x%x", faultadr, errcode);
}

int
usr_page_fault(uint errcode, int faultadr) {
  register proc_t *pp = proc;
  int  flt;
  seg_t    *seg;
  vas_t     *vas;
  enum seg_rw rw;

  proc_t* cur = proc;

  //cprintf("proc:%d pgflt addr:0x%x err:0x%x\n", proc->pid, faultadr, errcode);

  if((vas = pp->vas) == NULL)
    panic("usr_page_fault: no vas allocated");

  seg = vas->segat(faultadr);

  if (errcode & PF_ERR_WRITE)
    rw = S_WRITE;
  else
    rw = S_READ;

  switch(errcode & PF_ERR_MASK) {
  case PF_ERR_PAGE:
    //if(seg == NULL)
      //panic("no seg");
    vas->fault(faultadr, 1, (enum fault_type) F_INVAL, rw);
    break;
  case PF_ERR_PROT:
    //if(errcode & PF_ERR_WRITE)
    {
      vas->fault(faultadr, 1, (enum fault_type)F_PROT, rw);
    }
    break;
  default:
    panic("impossible condition");
    break;
  }

  return 0;
}

extern "C"
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_PGFLT:
    {
      uint faultadr = rcr2();
      uint errorcode = tf->err;

      if(errorcode & PF_ERR_USER)
        usr_page_fault(errorcode, (int)faultadr);
      else
        kern_page_fault(errorcode, (int)faultadr);
    }
    break;
  case T_GPFLT:
  {
    uint faultadr = rcr2();
    uint errorcode = tf->err;
    cprintf("gpflt addr:0x%x err:0x%x\n", faultadr, errorcode);
    panic("");
    break;
  }
  case T_IRQ0 + IRQ_TIMER:
    if(cpu->id == 0){
      ticks++;
      wakeup(&ticks);
    }
    break;
  case T_IRQ0 + IRQ_IDE:
    ide.intr();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    break;
  case T_IRQ0 + IRQ_COM1:
    //uartintr();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    break;
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      //panic("trap");
      return;
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip,
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}



