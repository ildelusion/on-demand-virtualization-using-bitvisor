#ifndef _CORE_SVM_DEVIRT_H_
#define _CORE_SVM_DEVIRT_H_

// #include "spinlock.h"	// If you use shared value, you have to include it.
#include "panic.h"
#include "printf.h"
#include "current.h"
#include "pcpu.h"
#include "asm.h"	// Load guest states to real cpu
#include "mm.h"		// For using memory map SHARED_AREA
#include "assert.h"	// ASSERT
#include "svm_vmcb.h"
#include "constants.h"	// CR4_PAE_BIT
#include "string.h"
#include "pt_walk.h"

//#define SHARED_AREA	0x48000000		// It is defined in constants.h
//#define SHARED_AREA_LEN 0x8000000		// It is defined in constants.h
#define MEMORY_DUMP_AREA 0x50000000
#define MEMORY_DUMP_AREA_AFTER 0x58000000
#define MEMORY_DUMP_AREA_LEN 0x8000000	// 128M
#define STATE_SAVE_SIZE	0x1000000	// state save size from shared area --> 0x48000000 ~ 0x49000000 (16MB)
#define	PAGE_TABLE_AREA	0x2000000
#define VMM_START_VIRT_ADDR 0x40000000
#define VMMSIZE_ALL (128 * 1024 * 1024)
#define NINE_BIT_MASK 0x00000000000001FF
#define REGISTER_VISIBLE 0		// If 1, print register values
#define _PRINT_FOR_DEBUG_DEVIRT_

#define RAX	0	// Shared Area Offset
#define RCX	1
#define RDX	2
#define RBX	3
#define CR2 4
#define RBP	5
#define RSI	6
#define RDI	7 
#define R8	8 
#define R9	9 
#define R10	10
#define R11	11
#define R12	12
#define R13	13
#define R14	14
#define R15	15

#define RSP	16
#define RFLAGS	17
#define RIP	18
#define CR0	19
#define CR3	21
#define CR4	22
//CR8 : 71

#define DR6	23
#define DR7	24
#define STAR	25
#define LSTAR	26
#define CSTAR	27
#define SFMASK	28
#define KERNEL_GS_BASE	29
#define SYSENTER_CS	30
#define SYSENTER_ESP	31
#define SYSENTER_EIP	32

#define PAT	33
#define EFER	34

// Segment Registers. It requires 128 bits.
#define CS_SEL		35		
#define CS_ATTR		36		// Hidden from software.
#define CS_LIMIT	37		// Hidden from software.
#define CS_BASE		38		// Hidden from software.

#define DS_SEL		39
#define DS_ATTR		40		// Hidden from software.
#define DS_LIMIT	41		// Hidden from software.
#define DS_BASE		42		// Hidden from software.

#define ES_SEL	 	43
#define ES_ATTR	 	44		// Hidden from software.
#define ES_LIMIT 	45		// Hidden from software.
#define ES_BASE	 	46		// Hidden from software.

#define FS_SEL		47
#define FS_ATTR		48		// Hidden from software.
#define FS_LIMIT	49		// Hidden from software.
#define FS_BASE		50

#define GS_SEL		51
#define GS_ATTR		52		// Hidden from software.
#define GS_LIMIT	53		// Hidden from software.
#define GS_BASE		54

#define SS_SEL		55
#define SS_ATTR		56		// Hidden from software.
#define SS_LIMIT	57		// Hidden from software.
#define SS_BASE		58		// Hidden from software.

#define GDTR_LIMIT	239		// For short *, 239*2 = 478
#define GDTR_BASE	60		// 60*8 = 480

#define LDTR_SEL	  	61
#define LDTR_ATTR	  	62		// Hidden from software.
#define LDTR_LIMIT  	63		// Hidden from software.
#define LDTR_BASE		64		// Hidden from software.

#define IDTR_LIMIT	263		// For short *, 263*2 = 526
#define IDTR_BASE	66		// 66*8 = 528

#define TR_SEL		67			// TSS Selector.	
#define TR_ATTR		68			// Hidden from software.
#define TR_LIMIT  	69			// Hidden from software.
#define TR_BASE		70			// Hidden from software.

#define CR8	71

#define GUEST_CR3_BACKUP	0x1000a98/8		// It will be added shared area (virtual address)
#define EMPTY_ENTRY_NUM_BACKUP	0x1000aa0/8	// It will be added shared area (virtual address)


static void
print_orig_registers(void)
{
	printf ("********** Original Register Values ***********\n"
			"rax: %-llx\n"
			"rcx: %-llx\n"
			"rdx: %-llx\n"
			"rbx: %-llx\n"
			"rbp: %-llx\n"
			"rsi: %-llx\n"
			"rdi: %-llx\n"
			"r8: %-llx\n"
			"r9: %-llx\n"
			"r10: %-llx\n"
			"r11: %-llx\n"
			"r12: %-llx\n"
			"r13: %-llx\n"
			"r14: %-llx\n"
			"r15: %-llx\n\n"
			"rsp: %-llx\n"
			"rflags: %-llx\n"
			"rip: %-llx\n"
			"cr0: %-llx\n"
			"cr2: %-llx\n"
			"cr3: %-llx\n"
			"cr4: %-llx\n"
			"cr8(tpr): %-llx\n\n"
			"dr6: %-llx\n"
			"dr7: %-llx\n"
			"star: %-llx\n"
			"lstar: %-llx\n"
			"cstar: %-llx\n"
			"sfmask: %-llx\n"
			"kernel_gs_base: %-llx\n"
			"sysenter_cs: %-llx\n"
			"sysenter_esp: %-llx\n"
			"sysenter_eip: %-llx\n\n"
			"pat: %-llx\n\n"
			"efer: %-llx\n\n"
			"cs.sel: %-x\n"
			"cs.attr: %-x\n"	// JS change it to -x from --llx
			"cs.limit: %-lx\n"
			"cs.base: %-llx\n"
			"ds.sel: %-x\n"
			"ds.attr: %-x\n"
			"ds.limit: %-lx\n"
			"ds.base: %-llx\n"
			"es.sel: %-x\n"
			"es.attr: %-x\n"
			"es.limit: %-lx\n"
			"es.base: %-llx\n"
			"fs.sel: %-x\n"
			"fs.attr: %-x\n"
			"fs.limit: %-lx\n"
			"fs.base: %-llx\n"
			"gs.sel: %-x\n"
			"gs.attr: %-x\n"
			"gs.limit: %-lx\n"
			"gs.base: %-llx\n"
			"ss.sel: %-x\n"
			"ss.attr: %-x\n"
			"ss.limit: %-lx\n"
			"ss.base: %-llx\n"
			"gdtr.limit: %-x\n"
			"gdtr.base: %-llx\n"
			"ldtr.sel: %-x\n"
			"ldtr.attr: %-x\n"
			"ldtr.limit: %-lx\n"
			"ldtr.base: %-llx\n"
			"idtr.limit: %-x\n"
			"idtr.base: %-llx\n"
			"tr.sel: %-x\n"
			"tr.attr: %-x\n"
			"tr.limit: %-lx\n"
			"tr.base: %-llx\n\n", \
			current->u.svm.vi.vmcb->rax, 
		current->u.svm.vr.rcx, 
		current->u.svm.vr.rdx, 
		current->u.svm.vr.rbx, 
		current->u.svm.vr.rbp, 
		current->u.svm.vr.rsi, 
		current->u.svm.vr.rdi, 
		current->u.svm.vr.r8, 
		current->u.svm.vr.r9, 
		current->u.svm.vr.r10, 
		current->u.svm.vr.r11, 
		current->u.svm.vr.r12, 
		current->u.svm.vr.r13, 
		current->u.svm.vr.r14, 
		current->u.svm.vr.r15, 
		current->u.svm.vi.vmcb->rsp, 
		current->u.svm.vi.vmcb->rflags, 
		current->u.svm.vi.vmcb->rip, 
		current->u.svm.vi.vmcb->cr0, 
		current->u.svm.vi.vmcb->cr2, 
		current->u.svm.vi.vmcb->cr3, 
		current->u.svm.vi.vmcb->cr4, 
		current->u.svm.vi.vmcb->v_tpr, 
		current->u.svm.vi.vmcb->dr6, 
		current->u.svm.vi.vmcb->dr7, 
		current->u.svm.vi.vmcb->star, 
		current->u.svm.vi.vmcb->lstar, 
		current->u.svm.vi.vmcb->cstar, 
		current->u.svm.vi.vmcb->sfmask, 
		current->u.svm.vi.vmcb->kernel_gs_base, 
		current->u.svm.vi.vmcb->sysenter_cs, 
		current->u.svm.vi.vmcb->sysenter_esp, 
		current->u.svm.vi.vmcb->sysenter_eip, 
		current->u.svm.vi.vmcb->g_pat, 
		current->u.svm.vi.vmcb->efer,
		current->u.svm.vi.vmcb->cs.sel, 
		current->u.svm.vi.vmcb->cs.attr, 
		current->u.svm.vi.vmcb->cs.limit, 
		current->u.svm.vi.vmcb->cs.base, 
		current->u.svm.vi.vmcb->ds.sel, 
		current->u.svm.vi.vmcb->ds.attr, 
		current->u.svm.vi.vmcb->ds.limit, 
		current->u.svm.vi.vmcb->ds.base, 
		current->u.svm.vi.vmcb->es.sel, 
		current->u.svm.vi.vmcb->es.attr, 
		current->u.svm.vi.vmcb->es.limit, 
		current->u.svm.vi.vmcb->es.base, 
		current->u.svm.vi.vmcb->fs.sel, 
		current->u.svm.vi.vmcb->fs.attr, 
		current->u.svm.vi.vmcb->fs.limit, 
		current->u.svm.vi.vmcb->fs.base, 
		current->u.svm.vi.vmcb->gs.sel, 
		current->u.svm.vi.vmcb->gs.attr, 
		current->u.svm.vi.vmcb->gs.limit, 
		current->u.svm.vi.vmcb->gs.base, 
		current->u.svm.vi.vmcb->ss.sel, 
		current->u.svm.vi.vmcb->ss.attr, 
		current->u.svm.vi.vmcb->ss.limit, 
		current->u.svm.vi.vmcb->ss.base, 
		current->u.svm.vi.vmcb->gdtr.limit, 
		current->u.svm.vi.vmcb->gdtr.base, 
		current->u.svm.vi.vmcb->ldtr.sel, 
		current->u.svm.vi.vmcb->ldtr.attr, 
		current->u.svm.vi.vmcb->ldtr.limit, 
		current->u.svm.vi.vmcb->ldtr.base, 
		current->u.svm.vi.vmcb->idtr.limit, 
		current->u.svm.vi.vmcb->idtr.base, 
		current->u.svm.vi.vmcb->tr.sel,
		current->u.svm.vi.vmcb->tr.attr,
		current->u.svm.vi.vmcb->tr.limit,
		current->u.svm.vi.vmcb->tr.base); 
}
	static void
print_registers(long *mem_long)
{
	short *mem_short = (short*)mem_long;

	printf ("********** Registers in Shared Area ***********\n"
			"rax: %-llx\n"
			"rcx: %-llx\n"
			"rdx: %-llx\n"
			"rbx: %-llx\n"
			"rbp: %-llx\n"
			"rsi: %-llx\n"
			"rdi: %-llx\n"
			"r8: %-llx\n"
			"r9: %-llx\n"
			"r10: %-llx\n"
			"r11: %-llx\n"
			"r12: %-llx\n"
			"r13: %-llx\n"
			"r14: %-llx\n"
			"r15: %-llx\n\n"
			"rsp: %-llx\n"
			"rflags: %-llx\n"
			"rip: %-llx\n"
			"cr0: %-llx\n"
			"cr2: %-llx\n"
			"cr3: %-llx\n"
			"cr4: %-llx\n"
			"cr8(tpr): %-llx\n\n"
			"dr6: %-llx\n"
			"dr7: %-llx\n"
			"star: %-llx\n"
			"lstar: %-llx\n"
			"cstar: %-llx\n"
			"sfmask: %-llx\n"
			"kernel_gs_base: %-llx\n"
			"sysenter_cs: %-llx\n"
			"sysenter_esp: %-llx\n"
			"sysenter_eip: %-llx\n\n"
			"pat: %-llx\n\n"
			"efer: %-llx\n\n"
			"cs_sel: %-llx\n"
			"cs_attr: %-llx\n"
			"cs_limit: %-llx\n"
			"cs_base: %-llx\n"
			"ds_sel: %-llx\n"
			"ds_attr: %-llx\n"
			"ds_limit: %-llx\n"
			"ds_base: %-llx\n"
			"es_sel: %-llx\n"
			"es_attr: %-llx\n"
			"es_limit: %-llx\n"
			"es_base: %-llx\n"
			"fs_sel: %-llx\n"
			"fs_attr: %-llx\n"
			"fs_limit: %-llx\n"
			"fs_base: %-llx\n"
			"gs_sel: %-llx\n"
			"gs_attr: %-llx\n"
			"gs_limit: %-llx\n"
			"gs_base: %-llx\n"
			"ss_sel: %-llx\n"
			"ss_attr: %-llx\n"
			"ss_limit: %-llx\n"
			"ss_base: %-llx\n"
			"gdtr_limit: %-llx\n"
			"gdtr_base: %-llx\n"
			"ldtr_sel: %-llx\n"
			"ldtr_attr: %-llx\n"
			"ldtr_limit: %-llx\n"
			"ldtr_base: %-llx\n"
			"idtr_limit: %-llx\n"
			"idtr_base: %-llx\n"
			"tr_sel: %-llx\n"
			"tr_attr: %-llx\n"
			"tr_limit: %-llx\n"
			"tr_base: %-llx\n\n",\
			*(mem_long + RAX), 
		*(mem_long + RCX), 
		*(mem_long + RDX), 
		*(mem_long + RBX), 
		*(mem_long + RBP), 
		*(mem_long + RSI), 
		*(mem_long + RDI), 
		*(mem_long + R8), 
		*(mem_long + R9), 
		*(mem_long + R10), 
		*(mem_long + R11), 
		*(mem_long + R12), 
		*(mem_long + R13), 
		*(mem_long + R14), 
		*(mem_long + R15),
		*(mem_long + RSP),
		*(mem_long + RFLAGS), 
		*(mem_long + RIP), 
		*(mem_long + CR0), 
		*(mem_long + CR2), 
		*(mem_long + CR3), 
		*(mem_long + CR4), 
		*(mem_long + CR8), 
		*(mem_long + DR6), 
		*(mem_long + DR7), 
		*(mem_long + STAR), 
		*(mem_long + LSTAR), 
		*(mem_long + CSTAR), 
		*(mem_long + SFMASK), 
		*(mem_long + KERNEL_GS_BASE), 
		*(mem_long + SYSENTER_CS), 
		*(mem_long + SYSENTER_ESP), 
		*(mem_long + SYSENTER_EIP),
		*(mem_long + PAT),
		*(mem_long + EFER),
		*(mem_long + CS_SEL), 
		*(mem_long + CS_ATTR), 
		*(mem_long + CS_LIMIT), 
		*(mem_long + CS_BASE), 
		*(mem_long + DS_SEL),   
		*(mem_long + DS_ATTR),  
		*(mem_long + DS_LIMIT), 
		*(mem_long + DS_BASE),  
		*(mem_long + ES_SEL),   
		*(mem_long + ES_ATTR),  
		*(mem_long + ES_LIMIT), 
		*(mem_long + ES_BASE),  
		*(mem_long + FS_SEL),   
		*(mem_long + FS_ATTR),  
		*(mem_long + FS_LIMIT), 
		*(mem_long + FS_BASE),  
		*(mem_long + GS_SEL),   
		*(mem_long + GS_ATTR),  
		*(mem_long + GS_LIMIT), 
		*(mem_long + GS_BASE),  
		*(mem_long + SS_SEL),   
		*(mem_long + SS_ATTR),  
		*(mem_long + SS_LIMIT), 
		*(mem_long + SS_BASE),  
		*(mem_short + GDTR_LIMIT), 
		*(mem_long + GDTR_BASE), 
		*(mem_long + LDTR_SEL),   
		*(mem_long + LDTR_ATTR),  
		*(mem_long + LDTR_LIMIT), 
		*(mem_long + LDTR_BASE),  
		*(mem_short+ IDTR_LIMIT), 
		*(mem_long + IDTR_BASE), 
		*(mem_long + TR_SEL),   
		*(mem_long + TR_ATTR),  
		*(mem_long + TR_LIMIT), 
		*(mem_long + TR_BASE)); 
}

// It returns virtual address of start_address
void svm_devirt (long *shared_area_vaddr);
void* svm_save_guest_register_to_shared_area (long *mem_long);
void* svm_find_level4_table_base_address (u64 cr3_register);
void* svm_memory_map_page_table (void* page_table_base_address);
int svm_find_empty_entry (long* g_page_table_base_address, long* h_page_table_base_address);
long* svm_alloc_table_to_entry (long* page_table_entry, u64 alloc_address);
void print_svm_exit_code (void);
long* svm_memory_map_in_vmm (u64 start_address, u64 len);


void svm_devirtualize ();
bool svm_devirtualizable ();
/* Map SHARED_AREA. JYKIM */
static long *
svm_map_shared_area(void) 
{
	long *mem_long = NULL;
	mem_long = svm_memory_map_in_vmm (SHARED_AREA, SHARED_AREA_LEN);
	// initialize 0x48000000 ~ 0x4FFFFFFF to zero	
	memset ((void*)mem_long, '\0', SHARED_AREA_LEN);
	return mem_long;
}

/* Unmap SHARED_AREA. JYKIM */
static void
svm_unmap_shared_area(void *shared_area_virt) 
{
	unmapmem(shared_area_virt, SHARED_AREA_LEN);
}
#endif
