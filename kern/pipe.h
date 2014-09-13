/*
 * pipe.h
 *
 */

#ifndef PIPE_H_
#define PIPE_H_

#include "types.h"

#define PIPESIZE 512

class file;

class pipe {
public:

  static int alloc(file**, file**);

  void close(int writable);

  int write(char *addr, int n);

  int read(char *addr, int n);

public:
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};


#endif /* PIPE_H_ */
