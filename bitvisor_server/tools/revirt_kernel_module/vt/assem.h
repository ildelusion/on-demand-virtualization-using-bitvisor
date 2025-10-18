#ifndef _VT_ASM_H
#define _VT_ASM_H

#define MSR_IA32_FS_BASE		0xC0000100
#define MSR_IA32_GS_BASE		0xC0000101
#define MSR_IA32_KERNEL_GS_BASE		0xC0000102
#define MSR_IA32_STAR			0xC0000081
#define MSR_IA32_LSTAR			0xC0000082
#define MSR_IA32_FMASK			0xC0000084
#define MSR_IA32_SYSENTER_CS		0x174
#define MSR_IA32_SYSENTER_ESP		0x175
#define MSR_IA32_SYSENTER_EIP		0x176

#define MSR_IA32_DEBUGCTL		0xC00001D9

struct rv_desc_ptr {
	unsigned short size;
	unsigned long address;
} __attribute__((packed)) ;

struct gprs_data{
	unsigned long rax, rcx, rdx, rbx, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15;
};

struct gprs_data data;


static inline void rv_read_rip(unsigned long *rip)
{
	    asm volatile ("lea (%%rip),%0" : "=r" (*rip));	// =r instead of rm
}

static inline unsigned long rv_read_dr6(void)
{
	unsigned long val;
	asm volatile("mov %%dr6,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline unsigned long rv_read_dr7(void)
{
	unsigned long val;
	asm volatile("mov %%dr7,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline unsigned long rv_read_cr0(void)
{
	unsigned long val;
	asm volatile("mov %%cr0,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline unsigned long rv_read_cr2(void)
{
	unsigned long val;
	asm volatile("mov %%cr2,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline unsigned long rv_read_cr3(void)
{
	unsigned long val;
	asm volatile("mov %%cr3,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;

}

static inline unsigned long rv_read_cr4(void)
{
	unsigned long val;
	asm volatile("mov %%cr4,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;

}

static inline unsigned long rv_read_cr8(void)
{
	unsigned long val;
	asm volatile("mov %%cr8,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;

}

static inline void rv_write_cr8(unsigned long val)
{
	asm volatile("mov %0,%%cr8\n\t" : :"r"(val));
}

// store gdt, idt, tr, ldt
static inline void rv_store_gdt(struct rv_desc_ptr *dtr)
{
	asm volatile("sgdt %0":"=m" (*dtr));
}

static inline void rv_store_idt(struct rv_desc_ptr *dtr)
{
	asm volatile("sidt %0":"=m" (*dtr));
}

static inline unsigned short rv_store_tr(void)
{
	unsigned short tr;

	asm volatile("str %0":"=r" (tr));

	return tr;
}

static inline unsigned short rv_store_ldt(void)	// It can be problem. I refer to the /lib/modules/3.16.37/build/arch/x86/include/asm
{
	unsigned short ldt;
	asm volatile("sldt %0":"=m" (ldt));
	return ldt;
}

// RFLAGS
static inline unsigned long rv_read_rflags(void)
{
	unsigned long val;
	asm volatile(
			"pushfq		\n\t"
			"popq %0		"
			: "=r" (val));
	//val = 0x0000000000000200 | val;
	return val;
}

// EFER
static inline unsigned long rv_read_efer(void)
{
	unsigned long edx;
	unsigned long eax;
	asm volatile(
			"mov $0xc0000080, %%ecx		\n\t"
			"rdmsr						\n\t"
			"mov %%edx, %0				\n\t"
			"mov %%eax, %1				"
			: "=m"(edx), "=m"(eax));
	return edx << 32 | (eax & 0x0000000011111111);
}

static inline unsigned short rv_read_cs(void)
{
	unsigned short cs;
	asm volatile(
			"mov %%cs, %0"
			: "=r"(cs));
	return cs;
}

static inline unsigned short rv_read_ds(void)
{
	unsigned short ds;
	asm volatile(
			"mov %%ds, %0"
			: "=r"(ds));
	return ds;
}

static inline unsigned short rv_read_es(void)
{
	unsigned short es;
	asm volatile(
			"mov %%es, %0"
			: "=r"(es));
	return es;
}

static inline unsigned short rv_read_fs(void)
{
	unsigned short fs;
	asm volatile(
			"mov %%fs, %0"
			: "=r"(fs));
	return fs;
}

static inline unsigned short rv_read_gs(void)
{
	unsigned short gs;
	asm volatile(
			"mov %%gs, %0"
			: "=r"(gs));
	return gs;
}

static inline unsigned short rv_read_ss(void)
{
	unsigned short ss;
	asm volatile(
			"mov %%ss, %0"
			: "=r"(ss));
	return ss;
}

static inline void rv_read_gprs(void)	// It should be called first among other inline assemlby code
{
	asm volatile(
		"push %%rax		\n\t"
		"push %%rcx		\n\t"
		"push %%rdx		\n\t"
		"push %%rbx		\n\t"
		"push %%rbp		\n\t"
		"push %%rsi		\n\t"
		"push %%rdi		\n\t"
		"push %%r8		\n\t"
		"push %%r9		\n\t"
		"push %%r10		\n\t"
		"push %%r11		\n\t"
		"push %%r12		\n\t"
		"push %%r13		\n\t"
		"push %%r14		\n\t"
		"push %%r15		\n\t"
		"pop %0			\n\t"
		"pop %1			\n\t"
		"pop %2			\n\t"
		"pop %3			\n\t"
		"pop %4			\n\t"
		"pop %5			\n\t"
		"pop %6			\n\t"
		"pop %7			\n\t"
		"pop %8			\n\t"
		"pop %9			\n\t"
		"pop %10		\n\t"
		"pop %11		\n\t"
		"pop %12		\n\t"
		"pop %13		\n\t"
		"pop %14		"
		:"=rm"(data.r15), "=rm"(data.r14), "=rm"(data.r13),
		"=rm"(data.r12), "=rm"(data.r11), "=rm"(data.r10), "=rm"(data.r9),
		"=rm"(data.r8), "=rm"(data.rdi), "=rm"(data.rsi), "=rm"(data.rbp),
		"=rm"(data.rbx), "=rm"(data.rdx), "=rm"(data.rcx), "=rm"(data.rax)); // push gprs except for rsp
	// pop them and save rsp
	// save rip
}

struct gprs_data rv_get_gprs_data(void)
{
	return data;
}

// read MSR
static inline unsigned long rv_read_msr(unsigned int msr_number)
{
	unsigned long edx;
	unsigned long eax;
	asm volatile(
			"mov %2, %%ecx		\n\t"
			"rdmsr						\n\t"
			"mov %%edx, %0				\n\t"
			"mov %%eax, %1				"
			: "=m"(edx), "=m"(eax)
			: "i"(msr_number));
	return edx << 32 | (eax & 0x00000000ffffffff);
}


// Read access right of segments.
static inline void
asm_lar (unsigned int sel, unsigned long int *ar)
{
	asm volatile ("lar %1,%0"
		      : "=r" (*ar)
		      : "rm" ((unsigned long int)sel)); /* avoid assembler bug */
}

/*#define ACCESS_RIGHTS_MASK		0xF0FF
#define ACCESS_RIGHTS_UNUSABLE_BIT	0x10000
unsigned int
get_seg_access_rights (unsigned short int sel)
{
	unsigned long int tmp, ret;

	if (sel) {
		asm_lar (sel, &tmp);
		ret = (tmp >> 8) & ACCESS_RIGHTS_MASK;
	} else {
		ret = ACCESS_RIGHTS_UNUSABLE_BIT;
	}
	return ret;
}
*/

// In case of general purpose register, we need to push them to stack first.
// save rip last.
// https://wiki.kldp.org/wiki.php/DocbookSgml/GCC_Inline_Assembly-KLDP 
// https://gcc.gnu.org/onlinedocs/
// You can see the constraints of the inline assemlby above links.

// ES, CS, SS, DS, FS, GS, GDTR, LDTR, IDTR, TR - GDTR_SEL_ATTR, IDTR_SEL_ATTR, GDTR_LMIT, IDTR_LIMIT
// EFER, CR4, CR3, CR0, DR7, DR6, RFLAGS, RIP, RSP, RAX

// STAR, LSTAR, CSTAR, SFMASK, KERNELGSBASE, SYSENTER_CS, SYSENTER_ESP, SYSENTER_EIP
// r15 ~ r8, rdi, rsi, rbp, rbx, rdx, rcx



#endif	// _VT_ASM_H
