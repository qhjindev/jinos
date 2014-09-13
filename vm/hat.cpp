/*
 * vm.cpp
 *
 */

#include "param.h"
#include "types.h"
#include "hat.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "kmem.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "mman.h"

extern char data[]; // defined by kernel.ld
pde_t *kpgdir; // for use in scheduler()
struct segdesc gdt[NSEGS];

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void seginit(void) {
  struct cpu *c = cpu;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  //c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu, and curproc
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);

  // Initialize cpu-local storage.
  //cpu = c;
  proc = 0;
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void switchkvm(void) {
  lcr3(v2p(kpgdir)); // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void switchuvm(struct proc *p) {
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint) proc->kstack + KSTACKSIZE;
  ltr(SEG_TSS << 3);
  if (p->get_hat()->get_pgdir() == 0)
    panic("switchuvm: no pgdir");
  lcr3(v2p(p->get_hat()->get_pgdir())); // switch to new address space
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void kvmalloc(void) {
  kpgdir = hat_t::setupkvm();
  switchkvm();
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
pte_t *
hat_t::walkpgdir(pde_t *pgdir, const void *va, int alloc) {
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if (*pde & PTE_P) {
    pgtab = (pte_t*) p2v(PTE_ADDR(*pde));
  } else {
    if (!alloc || (pgtab = (pte_t*) kmem.alloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = v2p(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

void hat_t::dumppgdir(pde_t* pgdir, uint vstart, uint vend) {

  pte_t *pte;

  uint vastart = PGROUNDDOWN(vstart);
  uint vaend = PGROUNDDOWN(vend);
  uint pa;
  uint flags;

  for (;;) {
    if ((pte = walkpgdir(pgdir, (const void*)vastart, 0)) == 0) {
      goto here;
    }

    if(*pte == 0)
      goto here;

    pa = *pte & ~0xFFF;
    flags = *pte & 0xFFF;
    cprintf("va:0x%x => pa:0x%x flags:0x%x\n", vastart, pa, flags);

here:
    if (vastart == vaend)
      break;
    vastart += PGSIZE;
  }
}

void
hat_t::dumppgdir(pde_t *pgdir) {
  pde_t *pde;
  pte_t *pgtab;
  pte_t *pg;
  for(int i=0; i<1024; i++) {
    pde = &pgdir[i];
    if (*pde & PTE_P) {
      pgtab = (pte_t*) p2v(PTE_ADDR(*pde));
      for(int j=0; j<1024; j++) {
        pg = &pgtab[j];
        if(*pg) {
          uint va = i << 22 | j << 12;
          uint pa = *pg & ~0xFFF;
          uint flags = *pg & 0xFFF;
          cprintf("va:0x%x => pa:0x%x flags:0x%x\n", va, pa, flags);
        }
      }
    }
  }
}

void
hat_t::mappage(pte_t* page_table, uint va, uint pa, int perm) {
  //cprintf("map va:0x%x to pa:0x%x for pte:0x%x\n", va, pa, *page_table);

  *page_table = pa | perm | PTE_P;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int hat_t::mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) {
  char *a, *last;
  pte_t *pte;

  //cprintf("map pgdir:0x%x va:0x%x size:0x%x to pa:0x%x\n", pgdir, va, size, pa);

  a = (char*) PGROUNDDOWN((uint)va);
  last = (char*) PGROUNDDOWN(((uint)va) + size - 1);
  for (;;) {
    if ((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if (*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

//Alloc hat_t and set up kernel part of a page table.
hat_t*
hat_t::alloc() {

  hat_t* hat = (hat_t*) kmem.alloc();
  if (hat == 0)
    panic("no mem alloc hat");

  pde_t* pgdir = setupkvm();

  hat->set_pgdir(pgdir);

  return hat;
}

// Set up kernel part of a page table.
pde_t*
hat_t::setupkvm(void) {
  pde_t *pgdir;
  struct kmap *k;

  if ((pgdir = (pde_t*) kmem.alloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);

  if (p2v(PHYSTOP) > (void*) DEVSPACE)
    panic("PHYSTOP too high");
  for (k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if (mappages(pgdir, k->virt, k->phys_end - k->phys_start, (uint) k->phys_start, k->perm) < 0)
      return 0;
  return pgdir;
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void hat_t::inituvm(char *init, uint sz) {
  char *mem;

  if (sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kmem.alloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, v2p(mem), PTE_W | PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int hat_t::loaduvm(char *addr, inode *ip, uint offset, uint sz) {
  uint i, pa, n;
  pte_t *pte;

  if ((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (ip->read((char*) p2v(pa), offset + i, n) != (int) n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int hat_t::allocuvm(uint oldsz, uint newsz) {
  char *mem;
  uint a;

  //cprintf("allocuvm pgdir:0x%x oldsz:0x%x newsz:0x%x\n", pgdir, oldsz, newsz);

  if (newsz >= KERNBASE)
    return 0;
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE) {
    mem = kmem.alloc();
    if (mem == 0) {
      cprintf("allocuvm out of memory\n");
      deallocuvm(newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    //cprintf("allocuvm pgdir:0x%x va:0x%x pa:0x%x\n", pgdir, a, v2p(mem));
    mappages(pgdir, (char*) a, PGSIZE, v2p(mem), PTE_W | PTE_U);
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int hat_t::deallocuvm(uint oldsz, uint newsz) {
  pte_t *pte;
  uint a, pa;

  //cprintf("deallocuvm pgdir:0x%x oldsz:0x%x newsz:0x%x\n", pgdir, oldsz, newsz);

  if (newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for (; a < oldsz; a += PGSIZE) {
    pte = walkpgdir(pgdir, (char*) a, 0);
    if (!pte)
      a += (NPTENTRIES - 1) * PGSIZE;
    else if ((*pte & PTE_P) != 0) {
      pa = PTE_ADDR(*pte);
      if (pa == 0)
        panic("kfree");
      //char *v = (char*) p2v(pa);
      //kmem.free(v);
      //cprintf("deallocuvm pgdir:0x%x va:0x%x pa:0x%x\n", pgdir, a, pa);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void hat_t::freevm() {
  uint i;

  //if(pgdir == 0)
  //panic("freevm: no pgdir");
  deallocuvm(KERNBASE, 0);
  for (i = 0; i < NPDENTRIES; i++) {
    if (pgdir[i] & PTE_P) {
      char * v = (char*) p2v(PTE_ADDR(pgdir[i]));
      kmem.free(v);
    }
  }
  kmem.free((char*) pgdir);
  flush_tlb();
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void hat_t::clearpteu(char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if (pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

int hat_t::dup(hat_t* ohat, seg_t* oldseg) {
  pte_t* pte;
  uint i, pa, flags;
  char* mem;

  int cow = (oldseg->type & MAP_PRIVATE);
  int start = oldseg->base;
  int end = oldseg->base + oldseg->size;

  for (i = start; i < end; ) {
    pte_t *src_pte, *dst_pte;
    src_pte = walkpgdir(ohat->get_pgdir(), (void*) i, 0);
    dst_pte = walkpgdir(pgdir, (void*)i, 1);

    pte_t pte = *src_pte;

    if (src_pte == 0)
      goto cont_copy_pte_noset;
    if (!(*src_pte & PTE_P))
      goto cont_copy_pte;

    //If it's a COW mapping, write protect it both in the parent and the child
    if(cow && (pte & PTE_W)) {
      *src_pte &= ~PTE_W;
      pte = *src_pte;
    }

    pa = PTE_ADDR(*src_pte);
    flags = PTE_FLAGS(*src_pte);

    //if ((mem = kmem.alloc()) == 0)
      //goto bad;

    //memmove(mem, (char*) p2v(pa), PGSIZE);

cont_copy_pte:  *dst_pte = pte;
cont_copy_pte_noset:    i += PGSIZE;
  }

  return 0;

bad:
  freevm();
  return 0;
}

// Given a parent process's page table, create a copy
// of it for a child.
hat_t*
hat_t::copyuvm(uint sz) {
  //pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  hat_t *nhat = hat_t::alloc();
  //if((d = setupkvm()) == 0)
  //return 0;
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if (!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kmem.alloc()) == 0)
      goto bad;
    memmove(mem, (char*) p2v(pa), PGSIZE);
    if (mappages(nhat->get_pgdir(), (void*) i, PGSIZE, v2p(mem), flags) < 0)
      goto bad;
  }
  return nhat;

bad:
  nhat->freevm();
  return 0;
}

// Map user virtual address to kernel address.
char*
hat_t::uva2ka(char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if ((*pte & PTE_P) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  return (char*) p2v(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int hat_t::copyout(uint va, void *p, uint len) {
  char *buf, *pa0;
  uint n, va0;

  buf = (char*) p;
  while (len > 0) {
    va0 = (uint) PGROUNDDOWN(va);
    pa0 = uva2ka((char*) va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if (n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

