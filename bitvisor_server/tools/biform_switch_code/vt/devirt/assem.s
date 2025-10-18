/* Constants. */
	OFFSET_RSP				= 0		
	OFFSET_RFLAGS			= 1
	OFFSET_RIP				= 2	
	OFFSET_CR0				= 3
	OFFSET_CR3				= 4
	OFFSET_CR4				= 5
	OFFSET_DR7				= 6
	OFFSET_EFER				= 7
	OFFSET_SYSENTER_CS		= 8
	OFFSET_SYSENTER_ESP		= 9
	OFFSET_SYSENTER_EIP		= 10	
	OFFSET_DEBUGCTL			= 11
	OFFSET_PAT				= 12
	OFFSET_PERF_GLOBAL		= 13
	OFFSET_BNDCFGS			= 14
# 	OFFSET_SMBASE			= 15
	OFFSET_CS_SEL			= 16	
	OFFSET_CS_ACCESS_RIGHT	= 17
	OFFSET_CS_LIMIT			= 18
	OFFSET_CS_BASE			= 19
	OFFSET_DS_SEL			= 20
	OFFSET_DS_ACCESS_RIGHT	= 21
	OFFSET_DS_LIMIT			= 22	
	OFFSET_DS_BASE			= 23
	OFFSET_ES_SEL			= 24
	OFFSET_ES_ACCESS_RIGHT	= 25
	OFFSET_ES_LIMIT			= 26
	OFFSET_ES_BASE			= 27
	OFFSET_FS_SEL			= 28
	OFFSET_FS_ACCESS_RIGHT	= 29
	OFFSET_FS_LIMIT			= 30
	OFFSET_FS_BASE			= 31
	OFFSET_GS_SEL			= 32
	OFFSET_GS_ACCESS_RIGHT	= 33
	OFFSET_GS_LIMIT			= 34
	OFFSET_GS_BASE			= 35
	OFFSET_SS_SEL			= 36
	OFFSET_SS_ACCESS_RIGHT	= 37
	OFFSET_SS_LIMIT			= 38
	OFFSET_SS_BASE			= 39
	OFFSET_GDTR_LIMIT		= 163	# For short *, 163*2=326
	OFFSET_GDTR_BASE		= 41	# 41*8 = 328
	OFFSET_LDTR_SEL			= 42
	OFFSET_LDTR_ACCESS_RIGHT= 43
	OFFSET_LDTR_LIMIT		= 44
	OFFSET_LDTR_BASE		= 45
	OFFSET_IDTR_LIMIT		= 187	# For short *, 187*2=374
	OFFSET_IDTR_BASE		= 47	# 47*8 = 376
	OFFSET_TR_SEL			= 48
	OFFSET_TR_ACCESS_RIGHT	= 49
	OFFSET_TR_LIMIT			= 50
	OFFSET_TR_BASE			= 51
	OFFSET_RAX				= 52
	OFFSET_RBX				= 53
	OFFSET_RCX				= 54
	OFFSET_RDX				= 55
	OFFSET_RDI				= 56
	OFFSET_RSI				= 57
	OFFSET_RBP				= 58
	OFFSET_R8 				= 59
	OFFSET_R9 				= 60
	OFFSET_R10				= 61
	OFFSET_R11				= 62
	OFFSET_R12				= 63
	OFFSET_R13				= 64
	OFFSET_R14				= 65
	OFFSET_R15				= 66
	OFFSET_CR2				= 67
# 	OFFSET_CR8				= 68
	OFFSET_STAR				= 69
	OFFSET_LSTAR			= 70
	OFFSET_FMASK			= 71
	OFFSET_KERNEL_GSBASE	= 72


/* Arguments passed by registers. */
# rdi = state save area (shared area)
# rdx = guest cr3

nop
nop

# descriptor table registers
# GDTR, IDTR
lgdt	2*OFFSET_GDTR_LIMIT(%rdi)		# 163 = 8*39.75, GDTR add 16 bit (1/4 * 64bit)
lidt	2*OFFSET_IDTR_LIMIT(%rdi)		# 187 = 8*45.75, IDTR

# RFLAGS
mov		8*OFFSET_RFLAGS(%rdi),%r10
and		$0xfffffffffffffdff,%r10	# Clear IF bit.
or		$0x0200000,%r10				# CPUID enabled.
push	%r10
popfq

# CR3
mov	%rdx,%cr3

# FS_BASE, GS_BASE
mov 8*OFFSET_FS_SEL(%rdi),%fs
mov 8*OFFSET_GS_SEL(%rdi),%gs

mov $0xc0000100,%ecx			# FS_BASE msr
mov 8*OFFSET_FS_BASE(%rdi),%eax
mov 252(%rdi),%edx 				# 252 = 8*31.5
wrmsr

mov $0xc0000101,%ecx			# GS_BASE msr
mov 8*OFFSET_GS_BASE(%rdi),%eax
mov 284(%rdi),%edx 				# 284 = 8*35.5
wrmsr

# MSRs
# Guest's STAR, LSTAR, FMASK, KERNEL_GSBASE is preserved in VMX-root mode.
# So, it is not needed to set those msr values.
#mov $0xc0000081,%ecx	# STAR
#mov 8*OFFSET_STAR(%rdi),%eax
#mov 556(%rdi),%edx	# 556 = 8*69.5
#wrmsr

#mov $0xc0000082,%ecx	# LSTAR
#mov 8*OFFSET_LSTAR(%rdi),%eax
#mov 564(%rdi),%edx	# 564 = 8*70.5
#wrmsr

#mov $0xc0000084,%ecx	# FMASK
#mov 8*OFFSET_FMASK(%rdi),%eax
#mov 572(%rdi),%edx	# 572 = 8*71.5
#wrmsr

#mov $0xc0000102,%ecx	# KERNEL_GSBASE
#mov 8*OFFSET_KERNEL_GSBASE(%rdi),%eax
#mov 580(%rdi),%edx	# 580 = 8*72.5
#wrmsr

mov $0x174,%ecx	# SYSENTER_CS
mov 8*OFFSET_SYSENTER_CS(%rdi),%eax
mov 68(%rdi),%edx	# 68 = 8*8.5
wrmsr

mov $0x175,%ecx	# SYSENTER_ESP
mov 8*OFFSET_SYSENTER_ESP(%rdi),%eax
mov 76(%rdi),%edx	# 76 = 8*9.5
wrmsr

mov $0x176,%ecx	# SYSENTER_EIP
mov 8*OFFSET_SYSENTER_EIP(%rdi),%eax
mov 84(%rdi),%edx	# 84 = 8*10.5
wrmsr

# DEBUGCTL, PERF_GLOBAL_CTRL, BDNCFGS msrs are failed to set.
# Those MSRs need not to be set.
#mov $0xc00001d9,%ecx	# DEBUGCTL
#mov 8*OFFSET_DEBUGCTL(%rdi),%eax
#mov 92(%rdi),%edx	# 92 = 8*11.5
#wrmsr

#mov $0xc000038f,%ecx	# PERF_GLOBAL_CTRL
#mov 8*OFFSET_PERF_GLOBAL(%rdi),%eax
#mov 108(%rdi),%edx	# 108 = 8*13.5
#wrmsr

#mov $0xc0000d90,%ecx	# BNDCFGS
#mov 8*OFFSET_BNDCFGS(%rdi),%eax
#mov 116(%rdi),%edx	# 116 = 8*14.5
#wrmsr

#mov $0x277,%ecx	# PAT	# You may be able to erase this. SAVE and LOAD IA32_PAT bit is unset in VM-Exit controls.
#mov 8*OFFSET_PAT(%rdi),%eax
#mov 100(%rdi),%edx	# 100 = 8*12.5
#wrmsr

# LDTR : It is not used in the test machine (64bit mode).
#lldt 8*OFFSET_LDTR_SEL(%rdi)

# debug registers
mov 8*OFFSET_DR7(%rdi),%rbx
mov %rbx,%dr7

# EFER
mov $0xc0000080,%ecx	# 0xc0000080 = EFER MSR number
mov 8*OFFSET_EFER(%rdi),%eax
mov 60(%rdi),%edx	# 60 = 8*7.5
wrmsr

# control registers
mov 8*OFFSET_CR0(%rdi),%rbx
mov %rbx,%cr0
mov 8*OFFSET_CR4(%rdi),%rbx
mov %rbx,%cr4
mov 8*OFFSET_CR2(%rdi),%rbx
mov %rbx,%cr2

# load selectors.
# DS, ES
mov 8*OFFSET_DS_SEL(%rdi),%ds
mov 8*OFFSET_ES_SEL(%rdi),%es
# SS
mov	$0,%rdx
mov %rdx,8*73(%rdi)
mov %rdx,8*74(%rdi)
lss	8*73(%rdi),%rdx

# Update Busy Bit of TSS desc.############################ [TR]
### load TSS desc linear addr to rcx.
mov 8*OFFSET_GDTR_BASE(%rdi), %rcx
add 8*OFFSET_TR_SEL(%rdi), %rcx	

### mov upper 32bit of TSS desc. to eax
mov 4(%rcx), %eax	 #64bit.
mov %eax, %ebx	## save original value.

### unset busy BIT.
and $0xfffffdff, %eax

### Mov eax value to TSS desc.
mov %eax, 4(%rcx)	#64bit

### Load TR.SEL
ltr 8*OFFSET_TR_SEL(%rdi)

### restore busy bit.
mov %ebx, 4(%rcx)	#64bit.	 original value is saved in ebx.
####################################################### [TR END]

# update rsp
mov 8*OFFSET_RSP(%rdi),%rsp
mov %rsp,%rbp

# push for IRETQ(cont') or LRETQ
pushq 8*OFFSET_CS_SEL(%rdi)
pushq 8*OFFSET_RIP(%rdi)

# general purpose registers
mov 8*OFFSET_RAX(%rdi),%rax
mov 8*OFFSET_RCX(%rdi),%rcx
mov 8*OFFSET_RDX(%rdi),%rdx
mov 8*OFFSET_RBX(%rdi),%rbx
mov 8*OFFSET_RBP(%rdi),%rbp
mov 8*OFFSET_RSI(%rdi),%rsi
mov 8*OFFSET_R8(%rdi),%r8
mov 8*OFFSET_R9(%rdi),%r9
mov 8*OFFSET_R10(%rdi),%r10
mov 8*OFFSET_R11(%rdi),%r11
mov 8*OFFSET_R12(%rdi),%r12
mov 8*OFFSET_R13(%rdi),%r13
mov 8*OFFSET_R14(%rdi),%r14
mov 8*OFFSET_R15(%rdi),%r15
mov 8*OFFSET_RDI(%rdi),%rdi

# Return.
lretq

nop
nop

### Serial Print (using stack) ##
# Store rax and rdx.#			#
pushq %rax						#
pushq %rdx						#
# Write to serial.				#
mov $0x41, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
# Check serial status.			#
add $0x5, %dx					#
1:								#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je 1b							#
# Restore rax, rdx.				#
popq %rdx						#
popq %rax						#
############ Serial END #########

#### Serial Print (original) ####
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x41, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_a:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_a		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########
