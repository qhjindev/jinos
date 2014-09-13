
#ifndef SYSFILE_H_
#define SYSFILE_H_

int sys_dup(void);
int sys_read(void);
int sys_write(void);
int sys_close(void);
int sys_fstat(void);
int sys_link(void);
int sys_unlink(void);
int sys_open(void);
int sys_mkdir(void);
int sys_mknod(void);
int sys_chdir(void);
int sys_exec(void);
int sys_pipe(void);
int sys_mmap(void);
int sys_munmap(void);

#endif /* SYSFILE_H_ */
