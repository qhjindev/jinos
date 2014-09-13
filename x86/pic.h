#ifndef PIC_H
#define PIC_H

#include "types.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)
#define IRQ_SLAVE       2       // IRQ at which slave connects to master

class pic_t {
public:
  void init();
  void enable(int irq);

private:
  void setmask(ushort mask);

  static ushort irqmask;
};

extern pic_t pic;

#endif //PIC_H
