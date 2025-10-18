#include "revirt.h"
#include "assem.h"

static void
print_sa (long *sa_vaddr)
{
	long *vaddr = 0;
	int i = 0;

	vaddr = sa_vaddr;
	printk (KERN_EMERG "\t\t**********************************\n");
	printk (KERN_EMERG "\t\t***** At guest Print Shared Area : %lx\n", sa_vaddr);
	printk (KERN_EMERG "\t\tindex : vaddr : value\n");
	while (i<64){
		printk (KERN_EMERG "\t\t%d : %016lx : %016lx\n", i, vaddr, *vaddr);
		vaddr++;
		i++;
	}
	printk (KERN_EMERG "\t\t***** Print Shared Area Done *****\n");
	printk (KERN_EMERG "\t\t**********************************\n");
}

static int 
__init revirt_init (void)
{
	unsigned long *sa_native_guest = 0;
	unsigned short *sa_native_guest_short;
	struct gprs_data data;
	unsigned long rsp = 0;
	struct rv_desc_ptr gdt;
	struct rv_desc_ptr idt;
	unsigned long rv_rip = 0;

	static bool is_revirted = false;
	unsigned long rflags_value;


	// Update kernel page table.
	update_kernel_page_table();

	// Locate switch code.
	locate_revirt_switch_code();
	
	// Clear IF bit. Host's IF bit is restored at the end of the revirt switch code
	// and guest's IF bit is restored when setting guest VMCB in VMM context.
	clear_if_bit();

	// Save native guest state.
	// shared area for native guest state.
	sa_native_guest = (unsigned long*) NATIVE_STATE_AREA;
	sa_native_guest_short = (unsigned short*) sa_native_guest;

	// Read current CPU state (Native guest state)
	rv_read_gprs();	
	data = rv_get_gprs_data();
	asm volatile("mov %%rsp,%0" : "=rm" (rsp));
	rv_store_gdt(&gdt);
	rv_store_idt(&idt);

	/** SAVE GUEST STATE **/
	// Guest state area in VMCS.
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RSP			) = rsp;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RFLAGS		) = rv_read_rflags();
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CR0			) = rv_read_cr0();
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CR3			) = rv_read_cr3();
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CR4			) = rv_read_cr4() | CR4_VMXE_BIT;	// Set gVMXE bit.
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DR7			) = rv_read_dr7();
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_EFER			) = 0xd01;	// Set value.
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_EFER			) = rv_read_efer();
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_CS	) = rv_read_msr(MSR_IA32_SYSENTER_CS);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_ESP	) = rv_read_msr(MSR_IA32_SYSENTER_ESP);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_EIP	) = rv_read_msr(MSR_IA32_SYSENTER_EIP);
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DEBUGCTL		) = rv_read_msr(MSR_IA32_DEBUGCTL);
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_PAT			) = gpat;	// TODO PAT??
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_PERF_GLOBAL	) = gperf_global;	//TODO PAT??
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_BNDCFGS		) = gbndcfgs;	//TODO
	// Do not save SMBASE. Assuming it is not changed.
	//smbase
	
	// Segment Registers
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CS_SEL		) = rv_read_cs();	//TODO??
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CS_ACCESS_RIGHT		) = 0x29b;	//TODO
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CS_LIMIT		) = guest_riv.cs.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CS_BASE		) = guest_riv.cs.base;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DS_SEL		) = rv_read_ds();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DS_ACCESS_RIGHT		) = guest_riv.ds.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DS_LIMIT		) = guest_riv.ds.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_DS_BASE		) = guest_riv.ds.base;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_ES_SEL		) = rv_read_es();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_ES_ACCESS_RIGHT		) = guest_riv.es.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_ES_LIMIT		) = guest_riv.es.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_ES_BASE		) = guest_riv.es.base;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_FS_SEL		) = rv_read_fs();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_FS_ACCESS_RIGHT		) = guest_riv.fs.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_FS_LIMIT		) = guest_riv.fs.limit;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_FS_BASE		) = rv_read_msr(MSR_IA32_FS_BASE);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_GS_SEL		) = rv_read_gs();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_GS_ACCESS_RIGHT		) = guest_riv.gs.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_GS_LIMIT		) = guest_riv.gs.limit;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_GS_BASE		) = rv_read_msr(MSR_IA32_GS_BASE);

	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SS_SEL		) = rv_read_ss();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SS_ACCESS_RIGHT		) = guest_riv.ss.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SS_LIMIT		) = guest_riv.ss.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_SS_BASE		) = guest_riv.ss.base;
	*(sa_native_guest_short + SHARED_AREA_VT_OFFSET_NATIVE_GDTR_LIMIT	) = gdt.size;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_GDTR_BASE	) = gdt.address;	
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LDTR_SEL		) = 0;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LDTR_SEL		) = rv_store_ldt();	//TODO
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LDTR_ACCESS_RIGHT	) = guest_riv.ldtr.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LDTR_LIMIT	) = guest_riv.ldtr.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LDTR_BASE	) = guest_riv.ldtr.base;
	*(sa_native_guest_short+ SHARED_AREA_VT_OFFSET_NATIVE_IDTR_LIMIT	) = idt.size;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_IDTR_BASE	) = idt.address;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_TR_SEL		) = rv_store_tr();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_TR_ACCESS_RIGHT		) = guest_riv.tr.acr;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_TR_LIMIT		) = guest_riv.tr.limit;
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_TR_BASE		) = guest_riv.tr.base;
	
	// The other vcpu registers.
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RAX			) = data.rax;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RBX			) = data.rbx;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RCX			) = data.rcx;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RDX			) = data.rdx;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RDI			) = data.rdi;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RSI			) = data.rsi;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RBP			) = data.rbp;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R8				) = data.r8;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R9				) = data.r9;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R10			) = data.r10;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R11			) = data.r11;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R12			) = data.r12;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R13			) = data.r13;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R14			) = data.r14;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_R15			) = data.r15;
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CR2			) = rv_read_cr2();
	//*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_CR8			) = rv_read_cr8();// Do not save.
	
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_STAR			) = rv_read_msr(MSR_IA32_STAR);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_LSTAR		) = rv_read_msr(MSR_IA32_LSTAR);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_FMASK		) = rv_read_msr(MSR_IA32_FMASK);
	*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_KERNEL_GSBASE) = rv_read_msr(MSR_IA32_KERNEL_GS_BASE);

	rv_read_rip(&rv_rip);

	if(is_revirted == false) {
		// Save RIP here because SHARED_AREA cannot be accessed after REVIRT.
		*(sa_native_guest + SHARED_AREA_VT_OFFSET_NATIVE_RIP ) = rv_rip;
		is_revirted = true;

		asm volatile(
				//"cli				\n\t"
				"mov %%cr4, %%r9 	\n\t"	// flush cache entries including global pages.
				"and $0xffffffffffffff7f, %%r9		\n\t"
				"mov %%r9, %%cr4	\n\t"
				"jmp %0"	// Jump to revirt switch code.
				: :"r"(REVIRT_SWITCH_CODE));
	}
	

	// Excuted after revirt done.
	set_if_bit();
	printk(KERN_EMERG "Revirt kernel module done.\n");
	return 0;
}

static void __exit revirt_exit(void)
{
	printk(KERN_EMERG "Exit VT revirt module.\n");
}

module_init(revirt_init);
module_exit(revirt_exit);
