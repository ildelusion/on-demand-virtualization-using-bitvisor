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

retfq

nop
nop

