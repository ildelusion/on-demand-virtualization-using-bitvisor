	.code64
	.global asm_jump_to_devirt_switch_code_64
	.align 16
asm_jump_to_devirt_switch_code_64:
	# stgi			# set GIF(Global Interrupt Flag)
	# RFLAGS.IF bit is already clear
	mov	%rsi,%rax	# arg2(rsi): physical address of guest vmcb
	mov	%rdi,%rcx	# arg1(rdi): start guest virtual address of switch code
	mov	%cr4,%r9
	and	$0xffffffffffffff7f,%r9
	mov	%r9,%cr4
	mov	8*21(%rdx),%rdx		# arg3(rdx): rdx is virtual address of shared area, 21 is cr3 constant
	add	$0x1000,%rdi	# guest virtual address of shared area i<<39 + 0x1000
	mov	$0xdeaddead, %r8
	mov	$0xdeaddead, %r9
	mov	$0xdeaddead, %r10
#mov	%cr3,%r8
	jmp	%rcx		# jmp to switch code
