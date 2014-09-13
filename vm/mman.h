/*
 * mman.h
 *
 */

#ifndef MMAN_H_
#define MMAN_H_

/*
 * Protections are chosen from these bits, or-ed together.
 * Note - not all implementations literally provide all possible
 * combinations.  PROT_WRITE is often implemented as (PROT_READ |
 * PROT_WRITE) and (PROT_EXECUTE as PROT_READ | PROT_EXECUTE).
 * However, no implementation will permit a write to succeed
 * where PROT_WRITE has not been set.  Also, no implementation will
 * allow any access to succeed where prot is specified as PROT_NONE.
 */
#define PROT_READ   0x1     /* pages can be read */
#define PROT_WRITE  0x2     /* pages can be written */
#define PROT_EXEC   0x4     /* pages can be executed */

#ifdef _KERNEL
#define PROT_USER   0x8     /* pages are user accessable */
#define PROT_ALL    (PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER)
#endif /* _KERNEL */

#define PROT_NONE   0x0     /* pages cannot be accessed */


/* sharing types:  must choose either SHARED or PRIVATE */
#define MAP_SHARED  1       /* share changes */
#define MAP_PRIVATE 2       /* changes are private */
#define MAP_TYPE    0xf     /* mask for share type */

/* other flags to mmap (or-ed in to MAP_SHARED or MAP_PRIVATE) */
#define MAP_FIXED   0x10        /* user assigns address */

/* these flags not yet implemented */
#define MAP_RENAME  0x20        /* rename private pages to file */
#define MAP_NORESERVE   0x40        /* don't reserve needed swap area */

/* these flags are used by memcntl */
#define PROC_TEXT   (PROT_EXEC | PROT_READ)
#define PROC_DATA   (PROT_READ | PROT_WRITE | PROT_EXEC)
#define SHARED      0x10
#define PRIVATE     0x20

#ifdef _KERNEL
#define PROT_EXCL   0x20
#endif /* _KERNEL */

#define VALID_ATTR  (PROT_READ|PROT_WRITE|PROT_EXEC|SHARED|PRIVATE)

int valid_va_range(int *basep, uint *lenp, uint minlen, int dir);
int valid_usr_range(int addr, size_t len);

class inode;

int do_mmap(inode* ip, uint addr, int len, int prot, int flags, int off);

#endif /* MMAN_H_ */
