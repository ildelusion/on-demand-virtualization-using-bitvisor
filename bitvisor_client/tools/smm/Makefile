CC = gcc
OBJCOPY = objcopy
CFLAGS = -Werror -fno-common 
PROGRAMS =  smihandler
SMM_LDSCRIPT = smm_tseg.ld
SMI_INCLUDE = -I. -I./include
BITS = 64
SMM_LDFLAGS  = -fPIC

OBJS = smihandler_assem.o smihandler_c.o lib.o smihandler_obj.o smihandler smihandler.elf


all : $(PROGRAMS)

smihandler_assem.o:
			$(CC) $(CFLAGS) -m$(BITS) -nostdlib $(SMM_LDFLAGS) -r -o $@ smihandler_assem.S
			
smihandler_c.o: 
			$(CC) $(CFLAGS) $(SMI_INCLUDE) -m$(BITS) -fomit-frame-pointer -nostdlib $(SMM_LDFLAGS) -r -o $@ smihandler_c.c 
			$(CC) $(CFLAGS) $(SMI_INCLUDE) -m$(BITS) -fomit-frame-pointer -nostdlib $(SMM_LDFLAGS) -r -S smihandler_c.c 

lib.o: 
			$(CC) $(CFLAGS) -g $(SMI_INCLUDE) -m$(BITS) -fomit-frame-pointer -nostdlib -nostdinc -fno-builtin $(SMM_LDFLAGS) -r -o $@ strings.c serial.c smiutil.c mm.c xmalloc.c rbtree.c timer.c

smihandler_obj.o : smihandler_c.o smihandler_assem.o lib.o  
			$(CC) $(CFLAGS) -m$(BITS) -nostdlib $(SMM_LDFLAGS) -r -o $@ $^

smihandler : smihandler_obj.o 
			$(CC) $(CFLAGS) -m$(BITS) $(SMM_LDFLAGS) -nostdlib -nostartfiles -static -o $@.elf -T $(SMM_LDSCRIPT) smihandler_obj.o
			$(OBJCOPY) -O binary $@.elf $@
	

clean:
	rm -rf $(PROGRAMS) $(OBJS)  *.s


cleanall:
	make clean
