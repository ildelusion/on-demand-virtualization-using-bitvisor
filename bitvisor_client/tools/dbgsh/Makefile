RM			= rm -f

.PHONY : all
all : dbgsh #dbgsh.exe	# uncomment dbgsh.exe to use MINGW

.PHONY : clean
clean :
	$(RM) dbgsh

dbgsh : dbgsh.c ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -o dbgsh dbgsh.c ../common/call_vmm.c
