/*
 * kmem.h
 *
 */

#ifndef KALLOC_H_
#define KALLOC_H_

#include "page_cache.h"

class kmem_t {
public:
  char* init();

  void init_range(void* vstart, void* vend);

  char* alloc(void);

  page_t* alloc_page(void);

  void free_page(page_t* p);

  void free(char* p);

private:
  page_t* freelist;
};

extern kmem_t kmem;

#endif /* KALLOC_H_ */
