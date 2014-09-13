
OBJS = buf.o exec.o file.o fs.o sysfile.o

vfs.o: $(OBJS)
	$(LD) -m elf_i386 -r -o vfs.o $(OBJS)

clean:
	rm -f *.o *.d mkfs