#ifndef PARAM_H
#define PARAM_H

#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NBUF         100  // size of disk block cache
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define LOGSIZE      10  // max data sectors in on-disk log

#define PAGESIZE 0x1000
#define PAGESHITF 12
#define PAGEOFFSET (PAGESIZE -1)
#define PAGEMASK (~PAGEOFFSET)

#ifndef NULL
#define NULL 0
#endif //NULL

#endif //PARAM_H

