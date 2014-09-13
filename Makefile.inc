TOPDIR = $(shell pwd)
CC = gcc
AS = gas
LD = ld
OBJCOPY = objcopy
OBJDUMP = objdump
INCDIR = -I$(TOPDIR) -I$(TOPDIR)/fs -I$(TOPDIR)/io -I$(TOPDIR)/kern -I$(TOPDIR)/lib -I$(TOPDIR)/vm -I$(TOPDIR)/x86
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32  -fno-omit-frame-pointer -fno-stack-protector
CFLAGS += $(INCDIR)
CPPFLAGS =  -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -fno-stack-protector -ffreestanding -Wextra -fno-exceptions -fno-rtti
CPPFLAGS += $(INCDIR)

ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
#LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null)

ARCHIVES = kern/kern.o vm/kvm.o fs/fs.o io/io.o x86/x86.o lib/lib.o
