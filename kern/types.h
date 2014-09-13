#ifndef TYPES_H
#define TYPES_H

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef uint pde_t;

typedef char* addr_t; /* ?<core address> type */
typedef char* caddr_t; /* ?<core address> type */
//typedef long daddr_t; /* <disk address> type */
typedef long off_t; /* ?<offset> type */
typedef short cnt_t; /* ?<count> type */

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#ifndef _SIZE_T
#define _SIZE_T
typedef uint size_t;
#endif

/* common macros */

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) < (b) ? (b) : (a))

#endif //TYPES_H
