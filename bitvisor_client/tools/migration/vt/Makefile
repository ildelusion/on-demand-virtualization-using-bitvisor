MINGW_PREFIX_DEBIAN	= i586-mingw32msvc-
MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
EXE_CC				= $(MINGW_PREFIX)cc
RM					= rm -f

obj-m += migrationModule.o

migrationModule-objs := migration_module.o assem.o

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
