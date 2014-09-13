
OBJS = ide.o kbd.o

io.o: $(OBJS)
	$(LD) -m elf_i386 -r -o io.o $(OBJS)

clean:
	rm -f *.o *.d