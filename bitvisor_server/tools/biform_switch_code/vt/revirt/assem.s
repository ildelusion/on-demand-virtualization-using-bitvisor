SHARED_AREA_HRIV				= 0xffff884049000800

OFFSET_HRIV_ES_SEL				= 0x00
OFFSET_HRIV_ES_LIMIT			= 0x08
OFFSET_HRIV_ES_ACCESS_RIGHT		= 0x10
OFFSET_HRIV_ES_BASE				= 0x18
OFFSET_HRIV_CS_SEL				= 0x20
OFFSET_HRIV_CS_LIMIT			= 0x28
OFFSET_HRIV_CS_ACCESS_RIGHT		= 0x30
OFFSET_HRIV_CS_BASE				= 0x38
OFFSET_HRIV_SS_SEL				= 0x40
OFFSET_HRIV_SS_LIMIT			= 0x48
OFFSET_HRIV_SS_ACCESS_RIGHT		= 0x50
OFFSET_HRIV_SS_BASE				= 0x58
OFFSET_HRIV_DS_SEL				= 0x60
OFFSET_HRIV_DS_LIMIT			= 0x68
OFFSET_HRIV_DS_ACCESS_RIGHT		= 0x70
OFFSET_HRIV_DS_BASE				= 0x78
OFFSET_HRIV_FS_SEL				= 0x80
OFFSET_HRIV_FS_LIMIT			= 0x88
OFFSET_HRIV_FS_ACCESS_RIGHT		= 0x90
OFFSET_HRIV_FS_BASE				= 0x98
OFFSET_HRIV_GS_SEL				= 0xa0
OFFSET_HRIV_GS_LIMIT			= 0xa8
OFFSET_HRIV_GS_ACCESS_RIGHT		= 0xb0
OFFSET_HRIV_GS_BASE				= 0xb8
OFFSET_HRIV_LDTR_SEL			= 0xc0
OFFSET_HRIV_LDTR_LIMIT			= 0xc8
OFFSET_HRIV_LDTR_ACCESS_RIGHT	= 0xd0
OFFSET_HRIV_LDTR_BASE			= 0xd8
OFFSET_HRIV_TR_SEL				= 0xe0
OFFSET_HRIV_TR_LIMIT			= 0xe8
OFFSET_HRIV_TR_ACCESS_RIGHT		= 0xf0
OFFSET_HRIV_TR_BASE				= 0xf8
OFFSET_HRIV_GDTR_LIMIT			= 0x100
OFFSET_HRIV_GDTR_BASE			= 0x108
OFFSET_HRIV_IDTR_LIMIT			= 0x110
OFFSET_HRIV_IDTR_BASE			= 0x118
OFFSET_HRIV_CR0					= 0x120
OFFSET_HRIV_CR3					= 0x128
OFFSET_HRIV_CR4					= 0x130
OFFSET_HRIV_DR7					= 0x138
OFFSET_HRIV_RFLAGS				= 0x140

OFFSET_HRIV_SS_TEMP1			= 0x148	# Used for load 0 to ss.
OFFSET_HRIV_SS_TEMP2			= 0x150	# Used for load 0 to ss.

OFFSET_HRIV_GDTR				= 0x106	# 0x100+0x6 = OFFSET_HRIV_GDTR_LIMIT+48/8
OFFSET_HRIV_IDTR				= 0x116	# 0x110+0x6 = OFFSET_HRIV_IDTR_LIMIT+48/8
OFFSET_HRIV_FS_BASE_HALF		= 0x9c	# OFFSET_HRIV_FS_BASE + 0x4
OFFSET_HRIV_GS_BASE_HALF		= 0xbc	# OFFSET_HRIV_GS_BASE + 0x4

SHARED_AREA_MISC				= 0xffff884049000a00

OFFSET_MISC_HOST_HRIV_VADDR		= 0x00
OFFSET_MISC_HOST_MISC_VADDR		= 0x08
# 0x10 is empty.
OFFSET_MISC_R15 				= 0x18
OFFSET_MISC_R14 				= 0x20
OFFSET_MISC_R13 				= 0x28
OFFSET_MISC_R12 				= 0x30
OFFSET_MISC_R11 				= 0x38
OFFSET_MISC_R10 				= 0x40
OFFSET_MISC_R9 					= 0x48
OFFSET_MISC_R8 					= 0x50
OFFSET_MISC_RDI 				= 0x58
OFFSET_MISC_RSI 				= 0x60
OFFSET_MISC_RBP 				= 0x68
OFFSET_MISC_RSP					= 0x70
OFFSET_MISC_RBX 				= 0x78
OFFSET_MISC_RDX 				= 0x80
OFFSET_MISC_RCX 				= 0x88
OFFSET_MISC_RAX 				= 0x90
OFFSET_MISC_RIP 				= 0x98
OFFSET_MISC_CR2					= 0xa0
OFFSET_MISC_CR8 				= 0xa8
OFFSET_MISC_SYSENTER_CS 		= 0xb0
OFFSET_MISC_SYSENTER_ESP 		= 0xb8
OFFSET_MISC_SYSENTER_EIP 		= 0xc0
OFFSET_MISC_EFER 				= 0xc8
OFFSET_MISC_PAT 				= 0xd0
OFFSET_MISC_STAR 				= 0xd8
OFFSET_MISC_LSTAR 				= 0xe0
OFFSET_MISC_FMASK 				= 0xe8
OFFSET_MISC_FS_BASE 			= 0xf0
OFFSET_MISC_GS_BASE 			= 0xf8
OFFSET_MISC_KERNEL_GS_BASE 		= 0x100
OFFSET_MISC_DEBUGCTL 			= 0x108
OFFSET_MISC_PERF_GLOBAL 		= 0x110
OFFSET_MISC_BNDCFGS 			= 0x118
OFFSET_MISC_VMCS_PTR_PA			= 0x120
OFFSET_MISC_GCR3_BAK			= 0x128
OFFSET_MISC_EMPTY_ENTRY_IDX_BAK	= 0x130

OFFSET_MISC_SYSENTER_CS_HALF 	= 0xb4
OFFSET_MISC_SYSENTER_ESP_HALF 	= 0xbc
OFFSET_MISC_SYSENTER_EIP_HALF 	= 0xc4
OFFSET_MISC_EFER_HALF 			= 0xcc

nop
nop

# Restore host state

## HRIV section -> %rdi
mov $SHARED_AREA_HRIV, %rdi
## MISC section -> %rsi
mov $SHARED_AREA_MISC, %rsi

# GDTR, IDTR
mov OFFSET_HRIV_GDTR_LIMIT(%rdi), %ax	# Set 80 bits. (LIMIT+BASE)
mov %ax, OFFSET_HRIV_GDTR(%rdi)
lgdt OFFSET_HRIV_GDTR(%rdi)

mov OFFSET_HRIV_IDTR_LIMIT(%rdi), %dx	# Set 80 bits. (LIMIT+BASE)
mov %dx, OFFSET_HRIV_IDTR(%rdi)	
lidt OFFSET_HRIV_IDTR(%rdi)

# RFLAGS
mov		OFFSET_HRIV_RFLAGS(%rdi),%r10
and		$0xfffffffffffffdff,%r10	# Clear IF bit.
#or		$0x0200000,%r10				# CPUID enabled.
push	%r10
popfq

# CR3
mov	OFFSET_HRIV_CR3(%rdi), %rdx
mov	%rdx, %cr3

# FS_BASE, GS_BASE
mov OFFSET_HRIV_FS_SEL(%rdi),%fs
mov OFFSET_HRIV_GS_SEL(%rdi),%gs

mov $0xc0000100,%ecx			# FS_BASE msr
mov OFFSET_HRIV_FS_BASE(%rdi),%eax
mov OFFSET_HRIV_FS_BASE_HALF(%rdi),%edx
wrmsr

mov $0xc0000101,%ecx			# GS_BASE msr
mov OFFSET_HRIV_GS_BASE(%rdi),%eax
mov OFFSET_HRIV_GS_BASE_HALF(%rdi),%edx
wrmsr

# MSRs
mov $0x174,%ecx	# SYSENTER_CS
mov OFFSET_MISC_SYSENTER_CS(%rsi),%eax
mov OFFSET_MISC_SYSENTER_CS_HALF(%rsi),%edx
wrmsr

mov $0x175,%ecx	# SYSENTER_ESP
mov OFFSET_MISC_SYSENTER_ESP(%rsi),%eax
mov OFFSET_MISC_SYSENTER_ESP_HALF(%rsi),%edx
wrmsr

mov $0x176,%ecx	# SYSENTER_EIP
mov OFFSET_MISC_SYSENTER_EIP(%rsi),%eax
mov OFFSET_MISC_SYSENTER_EIP_HALF(%rsi),%edx
wrmsr

# Debug register
mov OFFSET_HRIV_DR7(%rdi),%rbx
mov %rbx,%dr7

# EFER
mov $0xc0000080,%ecx	# 0xc0000080 = EFER MSR number
mov OFFSET_MISC_EFER(%rsi),%eax
mov OFFSET_MISC_EFER_HALF(%rsi),%edx
wrmsr

# control registers
mov OFFSET_HRIV_CR0(%rdi),%rbx
mov %rbx,%cr0
mov OFFSET_HRIV_CR4(%rdi),%rbx
mov %rbx,%cr4
mov OFFSET_MISC_CR2(%rsi),%rbx
mov %rbx,%cr2

# DS, ES
mov OFFSET_HRIV_DS_SEL(%rdi),%ds
mov OFFSET_HRIV_ES_SEL(%rdi),%es

# SS
mov	$0, %rdx
mov %rdx, OFFSET_HRIV_SS_TEMP1(%rdi)
mov %rdx, OFFSET_HRIV_SS_TEMP2(%rdi)
lss	OFFSET_HRIV_SS_TEMP1(%rdi), %rdx

# Update Busy Bit of TSS desc.############################ [TR]
### load TSS desc linear addr to rcx.
mov OFFSET_HRIV_GDTR_BASE(%rdi), %rcx
add OFFSET_HRIV_TR_SEL(%rdi), %rcx	

### mov upper 32bit of TSS desc. to eax
mov 4(%rcx), %eax	 #64bit.
mov %eax, %ebx	## save original value.

### unset busy BIT.
and $0xfffffdff, %eax

### Mov eax value to TSS desc.
mov %eax, 4(%rcx)	#64bit

### Load TR.SEL
ltr OFFSET_HRIV_TR_SEL(%rdi)

### restore busy bit.
mov %ebx, 4(%rcx)	#64bit.	 original value is saved in ebx.
####################################################### [TR END]

# update rsp
mov OFFSET_MISC_RSP(%rsi),%rsp
mov %rsp,%rbp

# push for IRETQ(cont') or LRETQ
pushq OFFSET_HRIV_CS_SEL(%rdi)
pushq OFFSET_MISC_RIP(%rsi)

# general purpose registers
mov OFFSET_MISC_RAX(%rsi),%rax
mov OFFSET_MISC_RCX(%rsi),%rcx
mov OFFSET_MISC_RDX(%rsi),%rdx
mov OFFSET_MISC_RBX(%rsi),%rbx
mov OFFSET_MISC_RBP(%rsi),%rbp
mov OFFSET_MISC_RDI(%rsi),%rdi
mov OFFSET_MISC_R8(%rsi),%r8
mov OFFSET_MISC_R9(%rsi),%r9
mov OFFSET_MISC_R10(%rsi),%r10
mov OFFSET_MISC_R11(%rsi),%r11
mov OFFSET_MISC_R12(%rsi),%r12
mov OFFSET_MISC_R13(%rsi),%r13
mov OFFSET_MISC_R14(%rsi),%r14
mov OFFSET_MISC_R15(%rsi),%r15
mov OFFSET_MISC_RSI(%rsi),%rsi

# Return.
lretq

nop
nop
