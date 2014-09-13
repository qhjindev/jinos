/*
 * vm.h
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#ifndef HAT_H_
#define HAT_H_

#include "mmu.h"

void seginit(void);

class inode;
class vas_t;
class seg_t;

void switchkvm(void);

void switchuvm(struct proc *p);

void kvmalloc(void);

//Hardware address translation, page table management
class hat_t {
public:
  static hat_t* alloc();

  //static void free(vas_t* vas);

  static pde_t* setupkvm(void);

  int dup(hat_t* ohat, seg_t* oldseg);

  //uint map(seg_t* seg, inode* vp, off_t vp_base, uint prot, uint flags);
  //void memload(seg_t* seg, int addr, page_t* pp, uint prot);
  //void unload(seg_t* seg, int addr, uint len, uint flags);

  void inituvm(char *init, uint sz);

  int loaduvm(char *addr, inode *ip, uint offset, uint sz);

  int allocuvm(uint oldsz, uint newsz);

  int deallocuvm(uint oldsz, uint newsz);

  void freevm();

  void clearpteu(char *uva);

  hat_t* copyuvm(uint sz);

  char* uva2ka(char *uva);

  int copyout(uint va, void *p, uint len);

  static pte_t* walkpgdir(pde_t *pgdir,const void *va, int alloc);

  static void dumppgdir(pde_t *pgdir);

  static void dumppgdir(pde_t* pgdir, uint vstart, uint vend);

  static void mappage(pte_t* page_table, uint va, uint pa, int perm);

public:
  void set_pgdir(pde_t* pgdir) {
    this->pgdir = pgdir;
  }

  pde_t* get_pgdir() {
    return pgdir;
  }

private:

  static int mappages(pde_t *pgdir,void *va, uint size, uint pa, int perm);

private:

  pde_t *pgdir;
};


#endif /* HAT_H_ */
