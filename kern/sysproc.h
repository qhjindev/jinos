/*
 * sysproc.h
 *
 */

#ifndef SYSPROC_H_
#define SYSPROC_H_

int sys_fork(void);

int sys_exit(void);

int sys_wait(void);

int sys_kill(void);

int sys_getpid(void);

int sys_sbrk(void);

int sys_sleep(void);

int sys_uptime(void);


#endif /* SYSPROC_H_ */
