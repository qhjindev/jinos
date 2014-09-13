#include "pipe.h"
#include "types.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "kmem.h"

int
pipe::alloc(file **f0, file **f1)
{
  pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = file::alloc()) == 0 || (*f1 = file::alloc()) == 0)
    goto bad;
  if((p = (pipe*)kmem.alloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  (*f0)->type = file::FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = file::FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
  return 0;

bad:
  if(p)
    kmem.free((char*)p);
  if(*f0)
    (*f0)->close();
  if(*f1)
    (*f1)->close();
  return -1;
}

void
pipe::close(int writable)
{
  if(writable){
    this->writeopen = 0;
    wakeup(&this->nread);
  } else {
    this->readopen = 0;
    wakeup(&this->nwrite);
  }
  if(this->readopen == 0 && this->writeopen == 0){
    kmem.free((char*)this);
  }
}

int
pipe::write(char *addr, int n)
{
  int i;

  for(i = 0; i < n; i++){
    while(this->nwrite == this->nread + PIPESIZE){  //DOC: pipewrite-full
      if(this->readopen == 0 || proc->killed){
        return -1;
      }
      wakeup(&this->nread);
      sleep(&this->nwrite);  //DOC: pipewrite-sleep
    }
    this->data[this->nwrite++ % PIPESIZE] = addr[i];
  }
  wakeup(&this->nread);  //DOC: pipewrite-wakeup1
  return n;
}

int
pipe::read(char *addr, int n)
{
  int i;

  while(this->nread == this->nwrite && this->writeopen){  //DOC: pipe-empty
    if(proc->killed){
      return -1;
    }
    sleep(&this->nread); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(this->nread == this->nwrite)
      break;
    addr[i] = this->data[this->nread++ % PIPESIZE];
  }
  wakeup(&this->nwrite);  //DOC: piperead-wakeup
  return i;
}

