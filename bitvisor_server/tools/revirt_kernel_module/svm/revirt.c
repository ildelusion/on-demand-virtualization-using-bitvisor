#include "revirt.h"
#include "assem.h"

static int __init revirt_init(void)
{
	static bool is_revirted = false;
	unsigned long rv_rip = 0;
	unsigned long rsp = 0;
	struct gprs_data data;
	struct rv_desc_ptr gdt;
	struct rv_desc_ptr idt;
	unsigned long rflags_value;

	unsigned long *state_save_area = (unsigned long*)STATE_SAVE_AREA;
	unsigned long *guest_vmcb_pa = (unsigned long*)GUEST_VMCB_PA_LOCATION;
	unsigned short *guest_vmcb_pa_short = NULL;

	update_kernel_page_table();
	printk(KERN_INFO "test kernel module 01\n");
	locate_revirt_switch_code();
	printk(KERN_INFO "test kernel module 02\n");

	guest_vmcb_pa += VMCB_START_STATE_SAVE_AREA;	// State save area starts at 0x400 of vmcb
	guest_vmcb_pa_short = (unsigned short*)guest_vmcb_pa;

	printk(KERN_INFO "test kernel module 03\n");
	save_host_gprs_into_host_state();
	//unsigned long rflags;
	printk(KERN_INFO "test kernel module 04\n");

	clear_if_bit();
	//rv_write_cr8(0xf);

	rv_read_gprs();
	printk(KERN_INFO "test kernel module 05\n");
	data = rv_get_gprs_data();
	asm volatile("mov %%rsp,%0" : "=rm" (rsp));
	rv_store_gdt(&gdt);
	rv_store_idt(&idt);
	// *(guest_vmcb_pa + GDTR_SEL_ATTR) = 0;
	*(guest_vmcb_pa_short + VMCB_GDTR_LIMIT) = gdt.size;
	*(guest_vmcb_pa + GDTR_BASE) = gdt.address;
	// *(guest_vmcb_pa + IDTR_SEL_ATTR) = 0;
	*(guest_vmcb_pa_short + VMCB_IDTR_LIMIT) = idt.size;
	*(guest_vmcb_pa + IDTR_BASE) = idt.address;
	printk(KERN_INFO "test kernel module 06\n");
	//*(guest_vmcb_pa + EFER) = rv_read_efer();
	*(guest_vmcb_pa + EFER) = 0x1d01;
	printk(KERN_INFO "test kernel module 06-1\n");
	printk(KERN_INFO "cr8 value: %lx\n", rv_read_cr8());
	*(guest_vmcb_pa + CR4) = rv_read_cr4();
	printk(KERN_INFO "test kernel module 06-2\n");
	*(guest_vmcb_pa + CR3) = rv_read_cr3();
	printk(KERN_INFO "test kernel module 06-3\n");
	*(guest_vmcb_pa + CR2) = rv_read_cr2(); printk(KERN_INFO "test kernel module 06-4\n");
	*(guest_vmcb_pa + CR0) = rv_read_cr0();
	printk(KERN_INFO "test kernel module 06-5\n");
	*(guest_vmcb_pa + RFLAGS) = rv_read_rflags();
	printk(KERN_INFO "test kernel module 06-6\n");
	*(guest_vmcb_pa + RSP) = rsp;
	printk(KERN_INFO "test kernel module 06-7\n");
	*(guest_vmcb_pa + RAX) = data.rax;
	printk(KERN_INFO "test kernel module 06-8\n");
	*(guest_vmcb_pa + FS_BASE_CONTENTS) = rv_read_msr(FS_base);
	printk(KERN_INFO "test kernel module 06-9\n");
	*(guest_vmcb_pa + GS_BASE_CONTENTS) = rv_read_msr(GS_base);
	printk(KERN_INFO "test kernel module 07\n");
	//*(guest_vmcb_pa_short + LDTR_SEL) = rv_store_ldt();
	*(guest_vmcb_pa_short + LDTR_SEL) = 0;
	*(guest_vmcb_pa_short + TR_SEL) = rv_store_tr();
	*(guest_vmcb_pa_short + CS_ATTR) = 0x29b;
	*(guest_vmcb_pa + DR7) = rv_read_dr7();
	*(guest_vmcb_pa + DR6) = rv_read_dr6();
	*(guest_vmcb_pa_short + ES) = rv_read_es();
	*(guest_vmcb_pa_short + DS_selector) = rv_read_ds();

	printk (KERN_INFO "GDT_LIMIT: %hx \n", *(guest_vmcb_pa_short + VMCB_GDTR_LIMIT));
	printk (KERN_INFO "GDT_BASE: %lx \n", *(guest_vmcb_pa + GDTR_BASE));
	printk (KERN_INFO "IDT_LIMIT: %hx \n", *(guest_vmcb_pa_short + VMCB_IDTR_LIMIT));
	printk (KERN_INFO "IDT_BASE: %lx \n", *(guest_vmcb_pa + IDTR_BASE));
	printk (KERN_INFO "EFER: %lx \n", *(guest_vmcb_pa + EFER));
	printk (KERN_INFO "CR4: %lx \n", *(guest_vmcb_pa + CR4));
	printk (KERN_INFO "CR3: %lx \n", *(guest_vmcb_pa + CR3));
	printk (KERN_INFO "CR0: %lx \n", *(guest_vmcb_pa + CR0));
	printk (KERN_INFO "RFLAGS: %lx \n", *(guest_vmcb_pa + RFLAGS));
	printk (KERN_INFO "RSP: %lx \n", *(guest_vmcb_pa + RSP));
	printk (KERN_INFO "RAX: %lx \n", *(guest_vmcb_pa + RAX));
	printk (KERN_INFO "FS_BASE: %lx \n", *(guest_vmcb_pa + FS_BASE_CONTENTS));
	//printk (KERN_INFO "GS_BASE: %lx \n", rv_read_msr(GS_base));		// This code has no error
	printk (KERN_INFO "GS_BASE: %lx \n", *(guest_vmcb_pa + GS_BASE_CONTENTS));
	printk (KERN_INFO "LDTR_SEL: %hx \n", *(guest_vmcb_pa_short + LDTR_SEL));
	printk (KERN_INFO "TR_SEL: %hx \n", *(guest_vmcb_pa_short + TR_SEL));
	printk (KERN_INFO "CS_ATTR: %hx \n", *(guest_vmcb_pa_short + CS_ATTR));
	printk (KERN_INFO "DR7: %lx \n", *(guest_vmcb_pa + DR7));
	printk (KERN_INFO "DR6: %lx \n", *(guest_vmcb_pa + DR6));
	printk (KERN_INFO "ES_SEL: %lx \n", *(guest_vmcb_pa_short + ES));
	printk (KERN_INFO "DS_SEL: %lx \n", *(guest_vmcb_pa_short + DS_selector));


	*(guest_vmcb_pa + STAR_MSR) = rv_read_msr(STAR);
	*(guest_vmcb_pa + LSTAR_MSR) = rv_read_msr(LSTAR);
	*(guest_vmcb_pa + CSTAR_MSR) = rv_read_msr(CSTAR);
	*(guest_vmcb_pa + SFMASK_MSR) = rv_read_msr(SFMASK);
	*(guest_vmcb_pa + SYSENTER_CS_MSR) = rv_read_msr(SYSENTER_CS);
	*(guest_vmcb_pa + SYSENTER_ESP_MSR) = rv_read_msr(SYSENTER_ESP);
	*(guest_vmcb_pa + SYSENTER_EIP_MSR) = rv_read_msr(SYSENTER_EIP);
	*(guest_vmcb_pa + KERNELGSBASE_MSR) = rv_read_msr(KernelGSBase);	// test it
	printk(KERN_INFO "test kernel module 02\n");
	*(guest_vmcb_pa + DEBUG_CTL_MSR) = rv_read_msr(DEBUG_CTL);
	*(guest_vmcb_pa + LAST_BRANCH_FROM_IP_MSR) = rv_read_msr(LAST_BRANCH_FROM_IP);
	*(guest_vmcb_pa + LAST_BRANCH_TO_IP_MSR) = rv_read_msr(LAST_BRANCH_TO_IP);
	*(guest_vmcb_pa + LAST_INT_FROM_IP_MSR) = rv_read_msr(LAST_INT_FROM_IP);
	*(guest_vmcb_pa + LAST_INT_TO_IP_MSR) = rv_read_msr(LAST_INT_TO_IP);

	printk (KERN_INFO "STAR_MSR: %lx \n", *(guest_vmcb_pa + STAR_MSR));
	printk (KERN_INFO "LSTAR_MSR: %lx \n", *(guest_vmcb_pa + LSTAR_MSR));
	printk (KERN_INFO "CSTAR_MSR: %lx \n", *(guest_vmcb_pa + CSTAR_MSR));
	printk (KERN_INFO "SFMASK_MSR: %lx \n", *(guest_vmcb_pa + SFMASK_MSR));
	printk (KERN_INFO "SYSENTER_CS_MSR: %lx \n", *(guest_vmcb_pa + SYSENTER_CS_MSR));
	printk (KERN_INFO "SYSENTER_ESP_MSR: %lx \n", *(guest_vmcb_pa + SYSENTER_ESP_MSR));
	printk (KERN_INFO "SYSENTER_EIP_MSR: %lx \n", *(guest_vmcb_pa + SYSENTER_EIP_MSR));
	printk (KERN_INFO "KERNELGSBASE_MSR: %lx \n", *(guest_vmcb_pa + KERNELGSBASE_MSR));
	printk (KERN_INFO "DEBUG_CTL_MSR: %lx \n", *(guest_vmcb_pa + DEBUG_CTL_MSR));
	printk (KERN_INFO "LAST_BRANCH_FROM_IP_MSR: %lx \n", *(guest_vmcb_pa + LAST_BRANCH_FROM_IP_MSR));
	printk (KERN_INFO "LAST_BRANCH_TO_IP_MSR: %lx \n", *(guest_vmcb_pa + LAST_BRANCH_TO_IP_MSR));
	printk (KERN_INFO "LAST_INT_FROM_IP_MSR: %lx \n", *(guest_vmcb_pa + LAST_INT_FROM_IP_MSR));
	printk (KERN_INFO "LAST_INT_TO_IP_MSR: %lx \n", *(guest_vmcb_pa + LAST_INT_TO_IP_MSR));

	*(state_save_area + STATE_SAVE_AREA_OFFSET_R15) = data.r15;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R14) = data.r14;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R13) = data.r13;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R12) = data.r12;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R11) = data.r11;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R10) = data.r10;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R9) = data.r9;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_R8) = data.r8;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RDI) = data.rdi;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RSI) = data.rsi;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RBP) = data.rbp;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RBX) = data.rbx;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RDX) = data.rdx;
	*(state_save_area + STATE_SAVE_AREA_OFFSET_RCX) = data.rcx;

	printk(KERN_INFO "test kernel module 03\n");
	rv_read_rip(&rv_rip);
	//set_if_bit();
	*(state_save_area + 0xe) = 0x8888fffc;
	printk(KERN_INFO "cr8 value: %lx\n", rv_read_cr8());
	//printk(KERN_INFO "after_rip_location\n");
	//rflags_value = rv_read_rflags();
	//printk(KERN_INFO "rflags: %lx\n", rflags_value);
	//printk(KERN_INFO "rv_rip: %lx\n", rv_rip);

	if(is_revirted == false) {
		//if(*(state_save_area + 0xe) == 0x8888fffc) {
		*(guest_vmcb_pa + RIP) = rv_rip;
		*(state_save_area + 0xe) = 0x8888fffb;
		is_revirted = true;
		//clear_if_bit();
		asm volatile(
//				"cli				\n\t"
				"mov %%cr4, %%r9 	\n\t"
				"and $0xffffffffffffff7f, %%r9		\n\t"
				"mov %%r9, %%cr4	\n\t"
				"jmp %0"
				: :"r"(REVIRT_SWITCH_CODE));
	} else {
		//set_if_bit();
		*(state_save_area + 0xe) = 0x8888ffff;
		printk(KERN_INFO "after revirt\n");
		*(state_save_area + 0xe) = 0x8888fffe;
	}

	*(state_save_area + 0xe) = 0x8888fffd;
	printk(KERN_INFO "revirt test\n");
	//rv_write_cr8(0x0);
	set_if_bit();
	//printk(KERN_INFO "devirt success\n");

	return 0;
	}

	static void __exit revirt_exit(void)
	{
		printk(KERN_INFO "Goodbye, world\n");
	}

	module_init(revirt_init);
	module_exit(revirt_exit);
