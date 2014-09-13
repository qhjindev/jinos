OBJS1 = \
	lib/string.o\
	x86/entry.o\
	x86/entrypgdir.o\
	x86/pic.o\
	x86/swtch.o\
	x86/trap.o\
	x86/trapasm.o\
	x86/vectors.o\
	io/ide.o\
	io/kbd.o\
	vm/kalloc.o\
	vm/vm.o\
	kern/console.o\
	kern/main.o\
	kern/pipe.o\
	kern/proc.o\
	kern/syscall.o\
	kern/sysproc.o\
	kern/timer.o\
	fs/buf.o\
	fs/exec.o\
	fs/file.o\
	fs/fs.o\
	fs/sysfile.o\

.EXPORT_ALL_VARIABLES:

TOPDIR = $(shell pwd)
CC = gcc
AS = gas
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump
INCDIR = -I$(TOPDIR) -I$(TOPDIR)/fs -I$(TOPDIR)/io -I$(TOPDIR)/kern -I$(TOPDIR)/lib -I$(TOPDIR)/vm -I$(TOPDIR)/x86
CFLAGS = -D_KERNEL -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32  -fno-omit-frame-pointer -fno-stack-protector
CFLAGS += $(INCDIR)
CPPFLAGS =  -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -fno-stack-protector -ffreestanding -Wextra -fno-exceptions -fno-rtti
CPPFLAGS += $(INCDIR)

ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
#LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)
LDFLAGS += -m elf_i386 

SUBDIRS = cmd kern vm fs io x86 lib
ARCHIVES = lib/lib.o x86/x86.o io/io.o vm/kvm.o kern/kern.o  fs/vfs.o  

subdirs:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i; done

jinos.img: kernel
	dd if=/dev/zero of=jinos.img count=10000
	dd if=kern/bootblock of=jinos.img conv=notrunc
	dd if=kernel of=jinos.img seek=1 conv=notrunc

kernel: $(ARCHIVES) kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld -o kernel $(ARCHIVES) -b binary initcode
	$(OBJDUMP) -t kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym

cmd/cmd.o:
	$(MAKE) -C cmd cmd.o
	
fs/vfs.o:
	$(MAKE) -C fs

io/io.o:
	$(MAKE) -C io
	
kern/kern.o:
	$(MAKE) -C kern
	
lib/lib.o:
	$(MAKE) -C lib

vm/kvm.o:
	$(MAKE) -C vm
	
x86/x86.o:
	$(MAKE) -C x86

tags: $(OBJS) _init
	etags *.S *.c


# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o


fs/mkfs: fs/mkfs.cpp fs/fs.h
	g++ -Werror -Wall -I./kern -o fs/mkfs fs/mkfs.cpp

fs.img: fs/mkfs 
	$(MAKE) -C cmd fs.img

#-include *.d

#clean: 
#	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*.o *.d *.asm *.sym bootblock \
	kern/initcode kern/initcode.out kernel jinos.img fs.img fs/mkfs \
	$(UPROGS)

all: jinos.img fs.img

jin:
	make clean
	make jinos.img
	make fs.img

clean:	
	@for dir in $(SUBDIRS); do \
		(cd $$dir; \
		make clean; ) \
	done
	rm ./kernel ./kernel.sym ./initcode ./*.img
#		rm -f *.o *.d *.asm *.sym mkfs bootblock initcode.out ); 
