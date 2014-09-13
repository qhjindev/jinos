
all: initcode x86.o

OBJS = entry.o entrypgdir.o swtch.o trap.o trapasm.o vectors.o pic.o

initcode: initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c initcode.S
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary initcode.out initcode
	mv initcode ..
	
x86.o: $(OBJS)
	$(LD) -m elf_i386 -r -o x86.o $(OBJS)

swtch.o:
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c swtch.S

entrypgdir.o:
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c entrypgdir.c

clean:
	rm -f *.o *.d initcode.out
