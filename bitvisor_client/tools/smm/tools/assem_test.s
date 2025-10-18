nop
nop

# SMM initialization
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
mov	%rax, %r12
mov	%rdx, %r13
# Write to serial.				#
mov $0x42, %al					#
mov $0x3f8, %dx					#
outb %al, %dx					#
								#
# Check serial status.			#
add $0x5, %dx					#
check_serial_status_b:			#
rep
nop
inb %dx, %al					#
and $0x20, %al					#
cmp $0x00, %al					#
je check_serial_status_b		#
								#
# Restore rax, rdx.				#
mov	%r12, %rax
mov	%r13, %rdx
############ Serial END #########

############## Serial Print ##### 
# Store rax and rdx.#           #
mov	%rax, %r12
mov	%rdx, %r13
# Write to serial.              #
mov $0x45, %al                  #
mov $0x3f8, %dx                 #
outb %al, %dx                   #
                                #
# Check serial status.          #
add $0x5, %dx                   #
check_serial_status_e:          #
rep
nop
inb %dx, %al                    #
and $0x20, %al                  #
cmp $0x00, %al                  #
je check_serial_status_e        #
                                #
# Restore rax, rdx.             #
mov	%r12, %rax
mov	%r13, %rdx
############ Serial END #########

# CR3 back up
mov	%cr3, %r8

# %rdi = HOST_GPRS_STATE
mov $0xffff880049000b00, %rdi

# clear CR4 g bit
mov %cr4, %r9
and $0xffffffffffffff7f, %r9
mov %r9, %cr4
  
# descriptor table registers # GDTR, IDTR
lgdt	2*0x6f(%rdi)
lidt	2*0x7f(%rdi)

# RFLAGS
mov	8*0x3d(%rdi), %r10
or	$0x0200000, %r10	# set ID bit for using CPUID instruction
push	%r10
popfq

# CR3 (switch to BitVisor Page Table)
mov 8*0x39(%rdi), %r9
mov %r9, %cr3
mov	$0xdeaddead, %r11

############## Serial Print ##### 
# Store rax and rdx.#           #
mov	%rax, %r12
mov	%rdx, %r13
# Write to serial.              #
mov $0x46, %al                  #
mov $0x3f8, %dx                 #
outb %al, %dx                   #
                                #
# Check serial status.          #
add $0x5, %dx                   #
check_serial_status_f:          #
rep
nop
inb %dx, %al                    #
and $0x20, %al                  #
cmp $0x00, %al                  #
je check_serial_status_f        #
# Restore rax, rdx.             #
mov	%r12, %rax
mov	%r13, %rdx
############ Serial END #########

# CR3 restore
mov	%r8, %cr3
mov	$0xdeaddead, %r10

############## Serial Print ##### 
# Store rax and rdx.#           #
mov	%rax, %r12
mov	%rdx, %r13
# Write to serial.              #
mov $0x47, %al                  #
mov $0x3f8, %dx                 #
outb %al, %dx                   #
                                #
# Check serial status.          #
add $0x5, %dx                   #
check_serial_status_g:          #
rep
nop
inb %dx, %al                    #
and $0x20, %al                  #
cmp $0x00, %al                  #
je check_serial_status_g        #
                                #
# Restore rax, rdx.             #
mov	%r12, %rax
mov	%r13, %rdx
############ Serial END #########

ret

nop
nop

