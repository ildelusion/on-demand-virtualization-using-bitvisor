/* Offset of Guest state. */
	CR3 = 4	 # SHARED_AREA_VT_OFFSET_CR3

	.code64
	.global asm_vt_jump_to_devirt_switch_code_64
	.align 16
asm_vt_jump_to_devirt_switch_code_64:
	mov	%rdi,%rcx	# arg1(rdi): start guest virtual address of switch code
	mov	%cr4,%r9
	and	$0xffffffffffffff7f,%r9
	mov	%r9,%cr4
	mov	8*CR3(%rsi),%rdx	# arg2(rsi): rsi is virtual address of shared area, 4 is cr3 constant
	add	$0x1000,%rdi	# guest virtual address of shared area i<<39 + 0x1000
	jmp	%rcx		# jmp to switch code
