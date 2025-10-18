#MINGW_PREFIX_DEBIAN	= i586-mingw32msvc-
#MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
#EXE_CC				= $(MINGW_PREFIX)cc
#RM					= rm -f

#obj-m += devirt.o devirt_2.o	// Generate 2 modules for debug.
obj-m += devirt.o

devirt-objs := hello.o
#devirt_2-objs := hello.o		// Generate 2 modules for debug.

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
