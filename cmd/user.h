struct stat;

// system calls
extern "C" int fork(void);
extern "C" int exit(void) __attribute__((noreturn));
extern "C" int wait(void);
extern "C" int pipe(int*);
extern "C" int write(int, void*, int);
extern "C" int read(int, void*, int);
extern "C" int close(int);
extern "C" int kill(int);
extern "C" int exec(char*, char**);
extern "C" int open(char*, int);
extern "C" int mknod(char*, short, short);
extern "C" int unlink(char*);
extern "C" int fstat(int fd, struct stat*);
extern "C" int link(char*, char*);
extern "C" int mkdir(char*);
extern "C" int chdir(char*);
extern "C" int dup(int);
extern "C" int getpid(void);
extern "C" char* sbrk(int);
extern "C" int mmap(int, int, int, int, int,int);
extern "C" int munmap(int, int);
extern "C" int sleep(int);
extern "C" int uptime(void);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
