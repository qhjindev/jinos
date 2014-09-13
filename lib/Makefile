
OBJS = string.o

lib.o: $(OBJS)
	$(LD) -m elf_i386 -r -o lib.o $(OBJS)

clean:
	rm -f *.o *.d
	