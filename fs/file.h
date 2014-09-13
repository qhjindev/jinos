/*
 * file.h
 */

#ifndef FILE_H_
#define FILE_H_

class inode;

class file {
public:
  enum { FD_NONE, FD_PIPE, FD_INODE } type;

  //void init();

  static file* alloc(void);

  void close();

  file* dup();

  int read(char*, int n);

  int write(char*, int n);

  int stat(struct stat*);

public:

  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  inode *ip;
  uint off;
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(inode*, char*, int);
  int (*write)(inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

int
exec(char *path, char **argv);


#endif /* FILE_H_ */
