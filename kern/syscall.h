/*
 * syscall.h
 *
 *  Created on: Aug 10, 2013
 *      Author: qinghua
 */

#ifndef SYSCALL_H_
#define SYSCALL_H_


int argint(int, int*);
int argptr(int, char**, int);
int argstr(int, char**);
int fetchint(uint, int*);
int fetchstr(uint, char**);
void syscall(void);

#endif /* SYSCALL_H_ */
