/*  
 *   *  hello-2.c - Demonstrating the module_init() and module_exit() macros.
 *   *  This is preferred over using init_module() and cleanup_module().
 *   */
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_EMERG */
#include <linux/init.h>		/* Needed for the macros */
#include "hello.h"
#include "assem.h"
#include <linux/delay.h>	// for msleep()

#define VMMCALL_NAME_MAXLEN 256

typedef struct {
	unsigned long rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	unsigned long rax, rbx, rcx, rdx, rsi, rdi;
} call_vmm_ret_t;

/**
 * Print 64bit value to serial.
 */
static void ascii_test(void)
{
	char hextoascii[]= "0123456789abcdef";
	long a = 0x1234abcd5678cdef;
	long temp_a = 0;
	int i = 15;

	clear_if_bit();
	for (i=15; i>=0; i--){
		temp_a = a;
		temp_a = temp_a >> (4*i);
		temp_a &= 0x000000000000000f;

		asm volatile(
				"mov %%rax, %%r12   \n\t"
				"mov %%rdx, %%r13   \n\t"
				"mov %0,    %%al    \n\t"
				"mov $0x3f8,    %%dx    \n\t"
				"outb %%al, %%dx    \n\t"
				"add $0x5,  %%dx    \n\t"
				"1: \n\t"
				"rep    \n\t"
				"nop    \n\t"
				"inb %%dx,  %%al    \n\t"
				"and $0x20, %%al    \n\t"
				"cmp $0x00, %%al    \n\t"
				"je 1b  \n\t"
				"mov %%r12, %%rax   \n\t"
				"mov %%r13, %%rdx   \n\t"
				:   
				: "g" (hextoascii[temp_a]));  // ASCII J.
	}
	set_if_bit();
}

static int __init hello_init(void)
{
	//unsigned long rflags;
	call_vmm_arg_t a;
	call_vmm_ret_t r;
	int devirt_vmmcall_number = 0;
	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "devirt";
	struct gprs_data data;
	struct rv_desc_ptr gdt;
	struct rv_desc_ptr idt;
	unsigned long rsp = 0;
	unsigned long rip = 0;

	printk(KERN_EMERG "kernel module starts\n");
/*
	ascii_test();
	printk(KERN_EMERG "kernel module ENDs\n");
	return 0;
	*/

	a.rbx = (long)vmmcall_name;
	
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (0),	// vmmcall_number == 0, It calls get_vmmcall vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	printk(KERN_EMERG "vmmcall_number is %d\n", r.rax);
	devirt_vmmcall_number = r.rax;

	// Print guest state before devirt request.
	rv_read_gprs();
	printk(KERN_EMERG "Print guest state before devirt request.\n");
	data = rv_get_gprs_data();
	asm volatile("mov %%rsp,%0" : "=rm" (rsp));
	rv_store_gdt(&gdt);
	rv_store_idt(&idt);
	// *(guest_vmcb_pa + GDTR_SEL_ATTR) = 0;
	// *(guest_vmcb_pa_short + VMCB_GDTR_LIMIT) = gdt.size;
	// *(guest_vmcb_pa + GDTR_BASE) = gdt.address;
	// *(guest_vmcb_pa + IDTR_SEL_ATTR) = 0;
	// *(guest_vmcb_pa_short + VMCB_IDTR_LIMIT) = idt.size;
	// *(guest_vmcb_pa + IDTR_BASE) = idt.address;
	// *(guest_vmcb_pa + EFER) = rv_read_efer();
	// *(guest_vmcb_pa + EFER) = 0x1d01;	// I used this value in svm revirtualization
	/*
	*(guest_vmcb_pa + CR4) = rv_read_cr4();
	*(guest_vmcb_pa + CR3) = rv_read_cr3();
	*(guest_vmcb_pa + CR2) = rv_read_cr2(); printk(KERN_EMERG "test kernel module 06-4\n");
	*(guest_vmcb_pa + CR0) = rv_read_cr0();
	*(guest_vmcb_pa + RFLAGS) = rv_read_rflags();
	*(guest_vmcb_pa + RSP) = rsp;
	*(guest_vmcb_pa + RAX) = data.rax;
	*(guest_vmcb_pa + FS_BASE_CONTENTS) = rv_read_msr(FS_base);
	*(guest_vmcb_pa + GS_BASE_CONTENTS) = rv_read_msr(GS_base);
	// *(guest_vmcb_pa_short + LDTR_SEL) = rv_store_ldt();
	*(guest_vmcb_pa_short + LDTR_SEL) = 0;
	*(guest_vmcb_pa_short + TR_SEL) = rv_store_tr();
	// *(guest_vmcb_pa_short + CS_ATTR) = 0x29b;
	*(guest_vmcb_pa + DR7) = rv_read_dr7();
	*(guest_vmcb_pa_short + ES) = rv_read_es();
	*(guest_vmcb_pa_short + DS_selector) = rv_read_ds();*/

	printk (KERN_EMERG "RSP: %lx \n", rsp);
	printk (KERN_EMERG "RFLAGS: %lx \n", rv_read_rflags());
	rv_read_rip(&rip);
	printk (KERN_EMERG "RIP: %lx \n", rip);
	printk (KERN_EMERG "CR0: %lx \n", rv_read_cr0());
	printk (KERN_EMERG "CR3: %lx \n", rv_read_cr3());
	printk (KERN_EMERG "CR4: %lx \n", rv_read_cr4());
	printk (KERN_EMERG "DR7: %lx \n", rv_read_dr7());
	printk (KERN_EMERG "EFER: %lx \n", rv_read_efer());
	// Originally in svm code, star ~ fmask are commented
	printk (KERN_EMERG "SYSENTER_CS_MSR: %lx \n", rv_read_msr(SYSENTER_CS));
	printk (KERN_EMERG "SYSENTER_ESP_MSR: %lx \n", rv_read_msr(SYSENTER_ESP));
	printk (KERN_EMERG "SYSENTER_EIP_MSR: %lx \n", rv_read_msr(SYSENTER_EIP));
	//printk (KERN_EMERG "DEBUG_CTL_MSR: %lx \n", rv_read_msr(DEBUG_CTL)); // error occurs
	printk (KERN_EMERG "PAT: %lx \n", rv_read_msr(PAT));
	//printk (KERN_EMERG "PERF_GLOBAL_CTRL: %lx \n", rv_read_msr(PERF_GLOBAL_CTRL));
	//printk (KERN_EMERG "BNDCFGS: %lx \n", rv_read_msr(BNDCFGS));
	printk (KERN_EMERG "CS_SEL: %lx \n", rv_read_cs());
	printk (KERN_EMERG "CS_ACR: %lx \n", get_seg_access_rights(rv_read_cs()));
	printk (KERN_EMERG "DS_SEL: %lx \n", rv_read_ds());
	//printk (KERN_EMERG "CS_ATTR: %hx \n", *(guest_vmcb_pa_short + CS_ATTR));
	printk (KERN_EMERG "ES_SEL: %lx \n", rv_read_es());
	printk (KERN_EMERG "FS_SEL: %lx \n", rv_read_fs());
	printk (KERN_EMERG "FS_BASE: %lx \n", rv_read_msr(FS_base));
	printk (KERN_EMERG "GS_SEL: %lx \n", rv_read_gs());
	printk (KERN_EMERG "GS_BASE: %lx \n", rv_read_msr(GS_base));		// This code has no error
	printk (KERN_EMERG "SS_SEL: %lx \n", rv_read_ss());		// This code has no error

	printk (KERN_EMERG "GDT_LIMIT: %hx \n", gdt.size);
	printk (KERN_EMERG "GDT_BASE: %lx \n", gdt.address);
	printk (KERN_EMERG "LDTR_SEL: %hx \n", rv_store_ldt());	// This code may have error
	printk (KERN_EMERG "IDT_LIMIT: %hx \n", idt.size);
	printk (KERN_EMERG "IDT_BASE: %lx \n", idt.address);
	printk (KERN_EMERG "TR_SEL: %hx \n", rv_store_tr());
	//printk (KERN_EMERG "GS_BASE: %lx \n", *(guest_vmcb_pa + GS_BASE_CONTENTS));

	//*(guest_vmcb_pa + KERNELGSBASE_MSR) = rv_read_msr(KernelGSBase);	// test it
	printk(KERN_EMERG "test kernel module 02\n");
	//*(guest_vmcb_pa + DEBUG_CTL_MSR) = rv_read_msr(DEBUG_CTL);

	printk (KERN_EMERG "gprs_rax: %lx\n", data.rax);
	printk (KERN_EMERG "gprs_rbx: %lx\n", data.rbx);
	printk (KERN_EMERG "gprs_rcx: %lx\n", data.rcx);
	printk (KERN_EMERG "gprs_rdx: %lx\n", data.rdx);
	printk (KERN_EMERG "gprs_rdi: %lx\n", data.rdi);
	printk (KERN_EMERG "gprs_rsi: %lx\n", data.rsi);
	printk (KERN_EMERG "gprs_rbp: %lx\n", data.rbp);
	printk (KERN_EMERG "gprs_r8: %lx\n", data.r8);
	printk (KERN_EMERG "gprs_r9: %lx\n", data.r9);
	printk (KERN_EMERG "gprs_r10: %lx\n", data.r10);
	printk (KERN_EMERG "gprs_r11: %lx\n", data.r11);
	printk (KERN_EMERG "gprs_r12: %lx\n", data.r12);
	printk (KERN_EMERG "gprs_r13: %lx\n", data.r13);
	printk (KERN_EMERG "gprs_r14: %lx\n", data.r14);
	printk (KERN_EMERG "gprs_r15: %lx\n", data.r15);
	printk (KERN_EMERG "cr2: %lx\n", rv_read_cr2());
	printk (KERN_EMERG "STAR_MSR: %lx \n", rv_read_msr(STAR));
	printk (KERN_EMERG "LSTAR_MSR: %lx \n", rv_read_msr(LSTAR));
	printk (KERN_EMERG "SFMASK_MSR: %lx \n", rv_read_msr(FMASK));
	// Originally in svm code, KERNELGSBASE is commented
	printk (KERN_EMERG "KERNELGSBASE_MSR: %lx \n", rv_read_msr(KernelGSBase));
	printk (KERN_EMERG "cr8 value: %lx\n", rv_read_cr8());
	printk (KERN_EMERG "VMX_CR0_FIXED0_MSR: %lx \n", rv_read_msr(VMX_CR0_FIXED0_MSR));
	printk (KERN_EMERG "VMX_CR0_FIXED1_MSR: %lx \n", rv_read_msr(VMX_CR0_FIXED1_MSR));
	printk (KERN_EMERG "VMX_CR4_FIXED0_MSR: %lx \n", rv_read_msr(VMX_CR4_FIXED0_MSR));
	printk (KERN_EMERG "VMX_CR4_FIXED1_MSR: %lx \n", rv_read_msr(VMX_CR4_FIXED1_MSR));

	clear_if_bit();
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (devirt_vmmcall_number),	// vmmcall_number. It is devirt vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");
			
	/*asm volatile(
			"mov %%rax, %%r12	\n\t"
			"mov %%rdx, %%r13	\n\t"
			"mov %0,	%%al	\n\t"
			"mov $0x3f8,	%%dx	\n\t"
			"outb %%al,	%%dx	\n\t"
			"add $0x5,	%%dx	\n\t"
			"1:	\n\t"
			"rep	\n\t"
			"nop	\n\t"
			"inb %%dx,	%%al	\n\t"
			"and $0x20,	%%al	\n\t"
			"cmp $0x00,	%%al	\n\t"
			"je 1b	\n\t"
			"mov %%r12, %%rax	\n\t"
			"mov %%r13, %%rdx	\n\t"
			:
			: "g" (0x49));	// ASCII I.
			*/

	set_if_bit();
	printk(KERN_EMERG "Devirt kernel module done.\n");
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_EMERG "Goodbye, world\n");
}

module_init(hello_init);
module_exit(hello_exit);
