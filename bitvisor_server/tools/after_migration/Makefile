MINGW_PREFIX_DEBIAN  = i586-mingw32msvc-
MINGW_PREFIX    = $(MINGW_PREFIX_DEBIAN)
EXE_CC      = $(MINGW_PREFIX)cc
RM      = rm -f

all : after_migration_vmmcall

after_migration_vmmcall : after_migration_vmmcall.c ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -o after_migration_vmmcall after_migration_vmmcall.c ../common/call_vmm.c

clean :
	$(RM) after_migration_vmmcall
