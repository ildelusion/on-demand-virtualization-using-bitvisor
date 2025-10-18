MINGW_PREFIX_DEBIAN  = i586-mingw32msvc-
MINGW_PREFIX    = $(MINGW_PREFIX_DEBIAN)
EXE_CC      = $(MINGW_PREFIX)cc
RM      = rm -f

all : test_vmmcall

test_vmmcall : test_vmmcall.c ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -o test_vmmcall test_vmmcall.c ../common/call_vmm.c

clean :
	$(RM) test_vmmcall
