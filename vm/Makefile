
OBJS = hat.o kmem.o mmap.o page_cache.o seg_vnode.o seg.o vas.o

kvm.o: $(OBJS)
	$(LD) -m elf_i386 -r -o kvm.o $(OBJS)

clean:
	rm -f *.o *.d
