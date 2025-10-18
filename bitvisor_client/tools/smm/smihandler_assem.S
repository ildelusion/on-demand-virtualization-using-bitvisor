/*
 * Note: These functions defined in this file may be called from C.
 *       Be careful of that you must not modify some registers. Quote
 *       from gcc-2.95.2/gcc/config/i386/i386.h:

   1 for registers not available across function calls.
   These must include the FIXED_REGISTERS and also any
   registers that can be used without being saved.
   The latter must include the registers where values are returned
   and the register where structure-value addresses are passed.
   Aside from that, you can include as many other registers as you like.

  ax,dx,cx,bx,si,di,bp,sp,st,st1,st2,st3,st4,st5,st6,st7,arg
{  1, 1, 1, 0, 0, 0, 0, 1, 1,  1,  1,  1,  1,  1,  1,  1,  1 }
 */

/*
 * Note: GRUB is compiled with the options -mrtd and -mregparm=3.
 *       So the first three arguments are passed in %eax, %edx, and %ecx,
 *       respectively, and if a function has a fixed number of arguments
 *       and the number if greater than three, the function must return
 *       with "ret $N" where N is ((the number of arguments) - 3) * 4.
 */

/*
 *  This is the area for all of the special variables.
 */

#include "lib/define.h"

#define SMM_STACK_SIZE	(SMM_TSEG_OFFSET - 0x10)
#define SMI_LOCKED	0
#define SMI_UNLOCKED	1

#define ENTRY(name)     \
		.global name @      \
		__ALIGN @       \
		name:

#define ENDPROC(name)       \
	.type name, @function @ \
	.size name, . - name


.section ".handler", "a", @progbits

.code16

smm_handler_start:


	movl	$(SMM_TSEG_ADDR), %edx
	movl    %edx, %ebx
	addl    $(smm_lock), %ebx
	movw 	$SMI_LOCKED, (%ebx)

	/* Get Core Num */
	mov		$LAPIC_ID, %esi
	mov		(%esi), %ecx
	shr		$24, %ecx				

	movl	%ecx, %eax
	shl		$1, %eax                                
	movl    %edx, %ebx
	addl    %eax, %ebx
	addl    $(smm_hold), %ebx
	movw 	$SMI_LOCKED, (%ebx)

	/* Compare */
	movl	$(SMM_CORE_SET_ADDR), %ebx
	movl	(%ebx), %eax
	cmpw	%ax, %cx
	je		smm_load_gdt
	
	movl    %edx, %ebx
	addl    $(smm_lock), %ebx

wait_for_unlock:

	pause
	movw 	(%ebx), %ax 
	cmpw    $SMI_LOCKED, %ax 
	je  wait_for_unlock

	movl	%ecx, %eax
	shl		$1, %eax                                

	movl    %edx, %ebx
	addl    %eax, %ebx
	addl    $(smm_hold), %ebx
	movw $SMI_UNLOCKED, (%ebx)
	
	rsm


	
smm_load_gdt:

	movl	$(SMM_TSEG_ADDR), %edx
	movl    $(smm_gdt_vector), %ebx
	addl	%edx, %ebx

	data32 lgdt (%ebx) 

	movl    %cr0, %eax
	andl    $0x1FFAFFD1, %eax /* CD,NW,PG,AM,WP,NE,TS,EM,MP = 0 */
	orl     $0x1, %eax        /* PE = 1 */
	movl    %eax, %cr0

	/* Enable protected mode */
	
	data32	jmp $GDT_ENTRY_32BIT_CS,$(smm_prot_start)


.code32
smm_prot_start:
	/* Use flat data segment */
	movw    $GDT_ENTRY_32BIT_DS, %ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss
	movw    %ax, %fs
	movw    %ax, %gs

	xor		%eax, %eax
	xor		%ebx, %ebx
	xor		%ecx, %ecx
	xor		%edx, %edx


	movl	$(SMM_TSEG_ADDR), %edx

	
	movl	%cr4, %eax
	orl		$1 << 5, %eax
	movl	%eax, %cr4
	
	movl    %edx, %ebx
	addl    $(pgtable_check), %ebx
	cmpw	$SMI_LOCKED, (%ebx)

	je pgtable_enabled

pgtable_initialize:

	/*
	 * Build early 4G identity pagetable
	 */
	/* Initialize Page tables to 0 */

	leal	pgtable(%edx), %edi	/* it points PML table */
	xorl	%eax, %eax
	movl	$((4096*2054)/4), %ecx
	rep	stosl

	/* Build Level 4 */
	/* PML4 Page- map Level 4 */

	leal	pgtable(%edx), %edi	/* it points PML table */
	/* 0x1000 = PDP base address, 0x0007 = Supervisor allow, Write allow, Present */
	leal	0x1007 (%edi), %eax  
	movl	%eax, 0(%edi)

	/* Build Level 3 */
	/* PDP Page Directory Pointer */

	leal	pgtable(%edx), %edi	
	addl	$0x1000,%edi
	/* 0x2000 = PD base address 0x0007 = Supervisor allow, Write allow, Present */
	leal	0x1007(%edi), %eax 

	movl	$4, %ecx	/* 4 entry setting */
1:	movl	%eax, 0(%edi)
	addl	$0x00001000, %eax /* next entry located in 4096 offset */
	addl	$8, %edi /* Each entry size = 8 bytes */
	decl	%ecx
	jnz	1b

	/* Build Level 2 */
	/* PD Page Directory */
	leal	pgtable(%edx), %edi	
	addl	$0x2000, %edi /* it points PD table */ 

	/* 0x3000 = Page Table base address 0x0007 = Supervisor allow, Write allow, Present */
	leal	0x4007(%edi), %eax 

	movl	$2048, %ecx
1:	movl	%eax, 0(%edi)
	addl	$0x00001000, %eax /* 4KB offset */
	addl	$8, %edi /* Each entry size = 8 bytes */
	decl	%ecx
	jnz	1b


	/* Build Level 1 */
	/* Page Table */
	leal	pgtable(%edx), %edi	
	addl	$0x6000, %edi /* it points PD table */ 

	/* 0x183 = Global Bit = 1, Write allow, Present */
	/* Global Page does is not invalidated in TLB entry */
	movl	$0x00000007, %eax
	movl	$1048576, %ecx /* 2048 * 512 = 1048576 */
1:	movl	%eax, 0(%edi)
	addl	$0x00001000, %eax /* 4KB offset */
	addl	$8, %edi /* Each entry size = 8 bytes */
	decl	%ecx
	jnz	1b

	/* setting the completion of page table contruction */
	movl    %edx, %ebx
	addl    $(pgtable_check), %ebx
	movw	$SMI_LOCKED, (%ebx)

	/* setting the completion of page table contruction */
	movl    %edx, %ebx
	addl    $(pgtable_completed), %ebx
	movw    $SMI_UNLOCKED, (%ebx)

pgtable_enabled:
	

	/* Enable the identity page tables */
	leal	pgtable(%edx), %eax	
	movl	%eax, %cr3

	/* Enable Long mode in EFER (Extended Feature Enable Register) */
	movl	$MSR_EFER, %ecx
	rdmsr
	btsl	$_EFER_LME, %eax
	wrmsr

	/* After gdt is loaded */
	/*
	xorl	%eax, %eax
	lldt	%ax
	*/


	/* Enter paged protected Mode, activating Long Mode */
	movl	%cr0, %eax
	orl	$(X86_CR0_PG), %eax /* Enable Paging and Protected mode */
	movl	%eax, %cr0


	jmp		$GDT_ENTRY_64BIT_CS,$(0x6000 + START_64BIT_ENTRY + SMM_TSEG_ADDR)

	/* Get SMM revision */
	
	movl	$0xfefc, %ebx			/* Save state of core 0 address */
	addl	%edx, %ebx				/* addjust for TSEG */
	subl	%ebp, %ebx				/* subtract core X offset */
	movl	(%ebx), %eax			/* SMM revision store stack */
	pushl	%eax

	/* Jump from 32bit compatibility mode into 64bit mode. */


	.code64
	.org START_64BIT_ENTRY
/* ENTRY(startup_64) */
startup_64:
	movl	$GDT_ENTRY_64BIT_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl   	%eax, %fs
	movl   	%eax, %gs
	movl   	%eax, %ss

	xor		%rax, %rax
	xor		%rcx, %rcx
	xor		%rbx, %rbx
	xor		%rdx, %rdx

	movq	$(SMM_TSEG_ADDR), %rdx

	mov		$LAPIC_ID, %esi
	mov		(%esi), %ecx
	shr		$24, %ecx				
	movl	%ecx, %ebx

	shl		$12, %rbx				
	movq	$0x8010, %rax	
	addq	%rdx, %rax		
	addq	%rbx, %rax		
	movq	%rax, %rbx		

	/*
	cld						
	mov		%rax, %rdi		
	mov		$(SMM_STACK_SIZE >> 2), %ecx	
	xor		%eax, %eax		
	rep	stosl				
	add		$SMM_STACK_SIZE, %rbx
	mov		%rbx, %rsp				
	mov		%rsp, %rbp				
	mov		$(SMM_ARGUMENT_ADDR - 0x10), %rsp
	*/

	add		$SMM_STACK_SIZE, %rbx

	mov		%rbx, %rsp				
	mov		%rsp, %rbp				

smi_call:
	
call	smi_handler
	


	
	/* SMI STATUS Clear */

/*
	mov		$(PMIO_COMMAND), %dx
	movb	$(PMIO_SMIRESULT), %al
	outb	%al, %dx
	mov		$(PMIO_DATA), %dx
	inb		%dx, %al
	outb	%al, %dx

	mov		$(PMIO_COMMAND), %dx
	movb	$(PMIO_MISCSTATUS), %al
	outb	%al, %dx
	mov		$(PMIO_DATA), %dx
	inb		%dx, %al
	outb	%al, %dx

	mov		$(PMIO_COMMAND), %dx
	movb	$(PMIO_WAKESMI_STATUS), %al
	outb	%al, %dx
	mov		$(PMIO_DATA), %dx
	inb		%dx, %al
	outb	%al, %dx

	mov		$(PMIO_COMMAND), %dx
	movb	$(PMIO_WAKEIRQ_STATUS), %al
	outb	%al, %dx
	mov		$(PMIO_DATA), %dx
	inb		%dx, %al
	outb	%al, %dx
	*/

	/* EOS Reset */
	/*
	mov		$(PMIO_COMMAND), %dx
	movb	$(PMIO_EOS), %al
	outb	%al, %dx
	mov		$(PMIO_DATA), %dx
	inb		%dx, %al
	orb		$(PMIO_EOS_BIT), %al
	outb	%al, %dx
	*/

/* for SINGLE CORE execution
	movl	$(SMM_TSEG_ADDR), %edx

smm_check_hold:

	movl	$0, %ecx	
	movl	%edx, %ebx
	addl    $(smm_hold), %ebx
1:	movl	%ecx, %eax
	shl		$1, %eax
	addl	%eax, %ebx
	addr32  movw (%ebx), %ax
	cmpw    $SMI_UNLOCKED, %ax 
	je		smm_check_hold
	cmpl	$(CPU-1), %ecx
	je		release_lock
	incl	%ecx
	jmp		1b
*/

release_lock:
	/* Release lock */


	movl	$(SMM_TSEG_ADDR), %edx 		
	movl    %edx, %ebx
	addl	$(smm_lock), %ebx
	movw	$SMI_UNLOCKED, %ax
	xchg	%ax, (%ebx)

	/* To return, just do rsm. It will "clean up" protected mode */
	rsm


  	

smm_gdt:
	/* The first GDT entry can not be used. Keep it zero */
	.long	0x00000000, 0x00000000

	/* gdt selector 0x08, flat code segment */
	.word	0xffff, 0x0000
	.byte	0x00, 0x9b, 0xcf, 0x40 /* G=1 and 0x0f, 4GB limit */

	/* gdt selector 0x10, flat data segment */
	.word	0xffff, 0x0000
	.byte	0x00, 0x93, 0xcf, 0x00

	/* 64bit Segment */
	.word	0xffff, 0x0000
	.byte	0x00, 0x9a, 0xaf, 0x00 

	.word	0xffff, 0x0000
	.byte	0x00, 0x92, 0xaf, 0x00


smm_gdt_end:

.section ".pgtable","a",@progbits
	.balign 4096

pgtable:
	.fill 3*4096, 1, 0xff



.section ".earlydata", "a", @progbits

.code16

.align	4, 0x00

smm_hold:
	.word SMI_UNLOCKED
	.word SMI_UNLOCKED
	.word SMI_UNLOCKED
	.word SMI_UNLOCKED
	.word SMI_UNLOCKED
	.word SMI_UNLOCKED

.align	4, 0x00
smm_lock:
	.word	SMI_UNLOCKED

.align	4, 0xff

smm_prot_vector:
	.long	smm_prot_start
	.short	8

.align	4, 0xff

smm_64_vector:
	.long	startup_64	
	.short	8

.align	4, 0xff


smm_gdt_addr:
	.word smm_gdt

.align	8, 0x00
smm_gdt_vector:
	.word	smm_gdt_end - smm_gdt - 1
	.long	SMM_TSEG_ADDR + smm_gdt

.align	4, 0xff

pgtable_check:
	.word	SMI_UNLOCKED
pgtable_completed:
	.word	SMI_UNLOCKED

.align	4, 0xff

.section ".emptytable", "a", @progbits
.align 0x4000, 0x00
empty_start:
	.word   0x0000
.align 0x2000, 0x00
empty_start2:
	.word   0x0000

.section ".jumptable", "a", @progbits

/* This is the SMM jump table. All cores use the same SMM handler
 * for simplicity. But SMM Entry needs to be different due to the
 * save state area. The jump table makes sure all CPUs jump into the
 * real handler on SMM entry.
 */

/* This code currently supports up to 16 CPU cores. If more than 16 CPU cores
 * shall be used, below table has to be updated, as well as smm_tseg.ld
 */

/* When using TSEG do a relative jump and fix up the CS later since we
 * do not know what our TSEG base is yet.
 */

.code16
jumptable:
.align SMM_TSEG_OFFSET, 0x00
	/* core 0 */
	jmp		smm_handler_start

.align SMM_TSEG_OFFSET, 0x00
	/* core 1 */
	jmp		smm_handler_start

.align SMM_TSEG_OFFSET, 0x00
	/* core 2 */
	jmp		smm_handler_start

.align SMM_TSEG_OFFSET, 0x00
	/* core 3 */
	jmp		smm_handler_start

.align SMM_TSEG_OFFSET, 0x00
	/* core 4 */
	jmp		smm_handler_start

.align SMM_TSEG_OFFSET, 0x00
	/* core 5 */
	jmp		smm_handler_start
