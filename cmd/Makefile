
#include ../Makefile.inc

#OBJS = init.o ls.o printf.o sh.o ulib.o umalloc.o usys.o

ULIB = ulib.o usys.o printf.o umalloc.o

usys.o:
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c usys.S

cmd.o: $(ULIB)
	$(LD) -m elf_i386 -r -o cmd.o ulib.o usys.o printf.o umalloc.o
	
_%: %.o cmd.o
	$(LD) $(LDFLAGS) -m elf_i386  -N -e main -Ttext 0 -o $@ $^

UPROGS=\
	_init\
	_ls\
	_sh\

fs.img: ../fs/mkfs $(UPROGS)
	../fs/mkfs fs.img $(UPROGS)
	mv fs.img ..

clean:
	rm -f *.o *.d _init _ls _sh
