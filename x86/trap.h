/*
 * trap.h
 *
 */

#ifndef TRAP_H_
#define TRAP_H_


void trapvectorinit(void);
void idtinit(void);
extern "C" void trap(struct trapframe *tf);




#endif /* TRAP_H_ */
