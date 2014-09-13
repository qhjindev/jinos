#ifndef IDE_H
#define IDE_H

#include "buf.h"

#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30

class ide_t {
public:
  void init();
  void intr(void);
  void rw(struct buf* b);

private:
  int wait(int checkerr);
  void start(struct buf*);
};

extern ide_t ide;

#endif //IDE_H
