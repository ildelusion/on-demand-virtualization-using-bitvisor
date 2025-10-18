#define HOST_GPRS_STATE	0xffff884049000b00
#define HOST_R15 0x0
#define HOST_R14 0x1
#define HOST_R13	0x2
#define HOST_R12	0x3
#define HOST_R11	0x4
#define HOST_R10	0x5
#define HOST_R9		0x6
#define HOST_R8		0x7
#define HOST_RDI	0x8
#define HOST_RSI	0x9
#define HOST_RBP	0xa
#define HOST_RSP	0xb
#define HOST_RBX	0xc
#define HOST_RDX	0xd
#define HOST_RCX	0xe
#define ES		0xf
#define CS		0x11
#define SS		0x13
#define DS		0x15
#define GDTR_LIMIT	0x6f	
#define IDTR_LIMIT	0x7f
#define EFER		0x29
#define CR4			0x38
#define	CR3 		0x39
#define CR0			0x3a
#define DR7			0x3b
#define DR6			0x3c
#define RFLAGS		0x3d
#define RIP			0x3e
#define RAX			0x4e

nop
nop

# SMM serial initialization
mov	$0x00, %al
mov	$0x3f1, %dx
outb	%al, %dx
mov	$0x03, %al
mov	$0x3f3, %dx
outb	%al, %dx
mov	$0x00, %al
mov	$0x3f2, %dx
outb	%al, %dx
mov	$0x03, %al
mov	$0x3f4, %dx
outb	%al, %dx
mov	$0x3f3, %dx
inb	%dx, %al
mov %al, %ah
or $0x80, %al
mov	$0x3f3, %dx
outb	%al, %dx
mov	$0x01, %al
mov	$0x3f0, %dx
outb	%al, %dx
mov	$0x0, %al
mov	$0x3f1, %dx
outb	%al, %dx
mov	%ah, %al
and $0x7f, %al
mov	$0x3f3, %dx
outb	%al, %dx

############## Serial Print	##### 
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

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x42, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_b:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_b		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# %rdi = HOST_GPRS_STATE
mov	$0xffff884049000b00, %rdi

# clear CR4 g bit
mov	%cr4, %r9
and $0xffffffffffffff7f, %r9
mov	%r9, %cr4

# descriptor table registers # GDTR, IDTR
lgdt	2*0x6f(%rdi)
lidt	2*0x7f(%rdi)

# RFLAGS
mov	8*0x3d(%rdi), %r10
or	$0x0200000, %r10	# set ID bit for using CPUID instruction
push	%r10
popfq

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x43, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_c:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_c		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# CR3 (switch to BitVisor Page Table)
mov	8*0x39(%rdi), %r9
mov	%r9, %cr3

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x44, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_d:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_d		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# vmload
mov	$0x49001000, %rax
vmload

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x45, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_e:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_e		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# ES, DS
mov	8*0xf(%rdi), %es
mov	8*0x15(%rdi), %ds

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x46, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_f:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_f		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# debug registers # DR7, DR6
mov	8*0x3b(%rdi), %rbx
mov	%rbx, %dr7
mov	8*0x3c(%rdi), %rbx
mov	%rbx, %dr6

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x47, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_g:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_g		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# MSR # EFER
mov	$0xc0000080, %ecx
mov	8*0x29(%rdi), %eax
mov	0x14c(%rdi), %edx	# 0x14c = 41.5 * 8	# 41 = 0x29
wrmsr

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x48, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_h:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_h		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# control registers # CR0, CR4
mov	8*0x3a(%rdi), %rbx
mov	%rbx, %cr0
mov	8*0x38(%rdi), %rbx
mov	%rbx, %cr4

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x49, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_i:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_i		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# lss # Stack segment selector
lss	8*0x13(%rdi), %rbx

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x4a, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_j:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_j		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# rsp, rbp
mov	8*0xb(%rdi), %rsp
mov	%rsp, %rbp

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x4b, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_k:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_k		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# push CS selector and RIP
xor	%rbx, %rbx
mov	8*0x11(%rdi), %bx
pushq	%rbx
pushq	8*0x3e(%rdi)

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x4c, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_l:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_l		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########

# GPRs
mov	8*0x4e(%rdi), %rax
mov	8*0xe(%rdi), %rcx
mov	8*0xd(%rdi), %rdx
mov 8*0xc(%rdi), %rbx
mov 8*0xa(%rdi), %rbp
mov 8*0x9(%rdi), %rsi
mov 8*0x7(%rdi), %r8
mov 8*0x6(%rdi), %r9
mov 8*0x5(%rdi), %r10
mov 8*0x4(%rdi), %r11
mov 8*0x3(%rdi), %r12
mov 8*0x2(%rdi), %r13
mov 8*0x1(%rdi), %r14
mov 8*0x0(%rdi), %r15
mov	8*0x8(%rdi), %rdi

sti	# add it 2016-10-26
retfq

nop
nop

############## Serial Print	##### 
# Store rax and rdx.#			#
mov	%rax, %r12					#
mov	%rdx, %r13					#
# Write to serial.				#
mov $0x4d, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_m:			#
rep								#
nop								#
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_m		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax					#
mov	%r13, %rdx					#
############ Serial END #########


/*
# Back up %rdi and %rax into 0x49000a00 --> 0xffff8800_49000a00
push %rdi
push %rbx
mov %rdi,%rbx
mov	$0xffff880049000a00,%rdi
mov	%rbx,0*8(%rdi)		# save original %rdi
mov %rax,1*8(%rdi)		# save original %rax
pop %rbx
pop %rdi
# Back up End

mov	$0xffff880049000800,%rdi
movw	0x10(%rdi),%rax
and $0x0ffff,%rax
pushq	%rax			# push CS selector
pushq	0x178(%rdi)		# push RIP

# Load original %rdi and %rax value from 0x49000900 --> 0xffff8800_49000900
mov	$0xffff880049000a00,%rdi
mov	1*8(%rdi),%rax
mov	0*8(%rdi),%rdi

retfq*/


