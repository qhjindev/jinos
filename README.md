jinos
=====

                                    Introduction
Jinos is a lightweight operating system developed by Qinghua Jin. The os kernel implements the basic and 
the most important features found in modern operation systems:

  IA-32 architecture support
  process management and schedule    
  virtual memory implement:
    hardware address translation (page table)
    demand paging and loading
    copy on write
    memory mapping
    code/data sharing between unrelated processes
    page cache
    file mapping
  filesystem impl
  object oriented design using c/c++/assembly language
  
DevEnv:
Ubuntu 10.04 or Redhat 6.2
kernel: linux-2.6.32
Qemu emulator

Build jinos:
make clean
make jinos.img
make fs.img

or 
make jin

Run jinos:
qemu-system-i386 -hdb fs.img -kernel kernel -smp 1 -m 512 jinos.img

or via vnc:
qemu-system-i386 -hdb fs.img -kernel kernel -smp 1 -m 512 jinos.img -vnc 0.0.0.0:10


