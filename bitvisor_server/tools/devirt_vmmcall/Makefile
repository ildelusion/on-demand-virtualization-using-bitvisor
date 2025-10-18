MINGW_PREFIX_DEBIAN  = i586-mingw32msvc-
MINGW_PREFIX    = $(MINGW_PREFIX_DEBIAN)
EXE_CC      = $(MINGW_PREFIX)cc
RM      = rm -f

all : devirt_vmmcall

devirt_vmmcall : devirt_vmmcall.c ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -o devirt_vmmcall devirt_vmmcall.c ../common/call_vmm.c

clean :
	$(RM) devirt_vmmcall
