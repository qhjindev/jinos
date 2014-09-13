
all: bootblock kern.o

OBJS = console.o main.o pipe.o proc.o syscall.o sysproc.o timer.o

bootblock: bootasm.S bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c bootmain.c
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootasm.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o bootblock.o bootasm.o bootmain.o
	$(OBJCOPY) -S -O binary -j .text bootblock.o bootblock
	./sign.pl bootblock

kern.o: $(OBJS)
	$(LD) -m elf_i386 -r -o kern.o $(OBJS)

clean:
	rm -f *.o *.d bootblock
	