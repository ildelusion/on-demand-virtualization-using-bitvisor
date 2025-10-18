nop
nop

# rdi = state save area (shared area)
# rax = address of guest vmcb
# rdx = guest cr3

# decriptor table registers
# GDTR, IDTR
lgdt	478(%rdi)		# 478 = 8*59.75, 59 -> GDTR add 16 bit (1/4 * 64bit)
lidt	526(%rdi)		# 526 = 8*65.75, 65 -> IDTR

# RFLAGS
mov		8*17(%rdi),%r10
and		$0xfffffffffffffdff,%r10
or	$0x0200000,%r10
push	%r10
popfq

# CR3
mov	%rdx,%cr3

# VMLOAD, guest vmcb address is in %rax
vmload

mov	8*39(%rdi),%ds		# DS_SEL = 39
mov	8*43(%rdi),%es		# ES_SEL = 43

# debug registers
mov	8*23(%rdi),%rbx	# DR6 = 23
mov	%rbx, %dr6
mov	8*24(%rdi),%rbx	# DR7 = 24
mov	%rbx, %dr7

# MSR
# EFER
mov	$0xc0000080,%ecx	# 0x0c0000080h = EFER MSR number
mov	8*34(%rdi),%eax	# EFER = 34
#and	$0xffffefff,%eax
mov	276(%rdi),%edx		# 276 = 8*34.5
# wrmsr writes the contents of the EDX:EAX register pair into a 64-bit model-specific register specified in the ECX register
wrmsr

# control registers
mov	8*19(%rdi),%rbx	# CR0 = 19
mov	%rbx,%cr0
mov	8*22(%rdi),%rbx	# CR4 = 22
mov	%rbx, %cr4
mov 8*4(%rdi),%rbx		# CR2 =4
mov	%rbx, %cr2
#mov	8*71(%rdi),%rbx	# CR8 = 71
#mov	%rbx, %cr8

mov	$0,%rdx
mov %rdx,8*98(%rdi)
mov %rdx,8*99(%rdi)
lss	8*98(%rdi),%rdx

mov	8*16(%rdi),%rsp		# RSP = 16, old rsp is useless, because I changed the CR3 register
mov	%rsp, %rbp

pushq	8*35(%rdi)		# CS_SEL = 35
pushq	8*18(%rdi)		# RIP = 18

# general purpose registers
mov	8*0(%rdi),%rax
mov	8*1(%rdi),%rcx
mov	8*2(%rdi),%rdx
mov	8*3(%rdi),%rbx
mov	8*5(%rdi),%rbp
mov	8*6(%rdi),%rsi
mov	8*8(%rdi),%r8
mov	8*9(%rdi),%r9
mov	8*10(%rdi),%r10
mov	8*11(%rdi),%r11
mov	8*12(%rdi),%r12
mov	8*13(%rdi),%r13
mov 8*14(%rdi),%r14
mov	8*15(%rdi),%r15
mov 8*7(%rdi),%rdi

#retfq
lretq

nop
nop

# mov cs, It cannot be written by mov instruction
#pushw	8*35(%rdi)		# CS = 35
#push	$1f;
#lretq
#1:

# FS.base, GS.base using MSR
#mov $0xc0000100,%ecx	# 0xc0000100h = FS.base
#	mov 8*50(%rdi),%eax		# FS_BASE = 50
#	mov 404(%rdi),%edx
#	wrmsr

#	mov	$0xc0000101,%ecx	# 0xc0000101h = GS.base
#	mov	8*54(%rdi),%eax		# GS_BASE = 54
#	mov	436(%rdi),%edx
#	wrmsr

#PAT
#wrmsr

#	mov	$0x0277, %ecx		# 0x0277 = PAT MSR number
#	mov	8*33(%rdi),%eax	# PAT = 33
#	mov	268(%rdi),%edx 	# 268 = 8*33.5
#	wrmsr
