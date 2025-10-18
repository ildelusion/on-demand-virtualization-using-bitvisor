#include "assert.h"
#include "constants.h"
#include "current.h"
#include "mm.h"
#include "pcpu.h"
#include "vt.h"
#include "vt_regs.h"
#include "vt_devirt.h"
#include "vt_asm.h"
#include "vt_vmcs.h"
#include "vmmcall_devirt.h"

//#define _PRINT_DEBUG_DEVIRT_VT_	// uncomment for DEBUG print.

#ifdef _PRINT_DEBUG_DEVIRT_VT_
	#define DEBUG_PRINT(x) printf x
#else
	#define DEBUG_PRINT(x) do {} while (0)
#endif

#define DEBUG_PRINT_VAL(x) DEBUG_PRINT(("[DEVIRT] (%s) %s:%lx\n", __func__, #x, x))

// For Test.
static void
vt_print_vrs(void)
{
	printf ("[REVIRT TEST] current->u.vt.vr.rax: %lx\n",current->u.vt.vr.rax);
	printf ("[REVIRT TEST] current->u.vt.vr.rcx: %lx\n",current->u.vt.vr.rcx);
	printf ("[REVIRT TEST] current->u.vt.vr.rbx: %lx\n",current->u.vt.vr.rbx);
	printf ("[REVIRT TEST] current->u.vt.vr.rdx: %lx\n",current->u.vt.vr.rdx);
	printf ("[REVIRT TEST] current->u.vt.vr.cr2: %lx\n",current->u.vt.vr.cr2);
	printf ("[REVIRT TEST] current->u.vt.vr.rbp: %lx\n",current->u.vt.vr.rbp);
	printf ("[REVIRT TEST] current->u.vt.vr.rsi: %lx\n",current->u.vt.vr.rsi);
	printf ("[REVIRT TEST] current->u.vt.vr.rdi: %lx\n",current->u.vt.vr.rdi);
	printf ("[REVIRT TEST] current->u.vt.vr.cr3: %lx\n",current->u.vt.vr.cr3);
	printf ("[REVIRT TEST] current->u.vt.vr.re: %d\n",current->u.vt.vr.re);
	printf ("[REVIRT TEST] current->u.vt.vr.pg: %d\n",current->u.vt.vr.pg);
	printf ("[REVIRT TEST] current->u.vt.vr.r8: %lx\n",current->u.vt.vr.r8);
	printf ("[REVIRT TEST] current->u.vt.vr.r9: %lx\n",current->u.vt.vr.r9);
	printf ("[REVIRT TEST] current->u.vt.vr.r10: %lx\n",current->u.vt.vr.r10);
	printf ("[REVIRT TEST] current->u.vt.vr.r11: %lx\n",current->u.vt.vr.r11);
	printf ("[REVIRT TEST] current->u.vt.vr.r12: %lx\n",current->u.vt.vr.r12);
	printf ("[REVIRT TEST] current->u.vt.vr.r13: %lx\n",current->u.vt.vr.r13);
	printf ("[REVIRT TEST] current->u.vt.vr.r14: %lx\n",current->u.vt.vr.r14);
	printf ("[REVIRT TEST] current->u.vt.vr.r15: %lx\n",current->u.vt.vr.r15);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.enable: %d\n",current->u.vt.vr.sw.enable);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.num: %d\n",current->u.vt.vr.sw.num);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.es: %lx\n",current->u.vt.vr.sw.es);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.cs: %lx\n",current->u.vt.vr.sw.cs);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.ss: %lx\n",current->u.vt.vr.sw.ss);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.ds: %lx\n",current->u.vt.vr.sw.ds);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.fs: %lx\n",current->u.vt.vr.sw.fs);
	printf ("[REVIRT TEST] current->u.vt.vr.sw.gs: %lx\n",current->u.vt.vr.sw.gs);
}

// For Test.
static void
print_sa (long *sa_vaddr)
{
	long *vaddr = 0;
	int i = 0;

	vaddr = sa_vaddr;
	printf ("**********************************\n");
	printf ("***** At VMM Print Shared Area : %lx\n", sa_vaddr);
	printf ("idx : vaddr : value\n");
	while (i<64){
		printf ("%d : %016lx : %016lx\n", i, vaddr, *vaddr);
		vaddr++;
		i++;
	}
	printf ("***** Print Shared Area Done *****\n");
	printf ("**********************************\n");
}

bool before_devirt;	// True: it is before devirt. False: it is after revirt.

/** Print for DEBUG.
 */
static void
vt_print_val(const char *func_name, long* mem_vaddr, char *name){
	DEBUG_PRINT(("[DEVIRT] (%s) %s\t%lx\n", func_name, name, *(mem_vaddr)));
}

// Make last 12 bits of PML4 table address 0 (zero)
static void*
vt_get_pml4_table_addr (u64 cr3_val)
{
	u64	pml4_table_addr = 0;
	cr3_val >>= 12;		// 51 - 12 bits are level4 table base address
	pml4_table_addr = cr3_val << 12;		// 52-bit is used

	return (void*)pml4_table_addr;
}

// Map physical memory for 4KB.
static void*
vt_mapmem_page_table (void* page_table_base_address)
{
	void *page_mem = NULL;

	page_mem = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, (u64)page_table_base_address, 0x1000);
	// 0x1000 = 4KB
	ASSERT (page_mem);

	return page_mem;
}


// Map physical memory with Size.
static void*
vt_mapmem_page (void* paddr, uint len)
{
	void *page_mem = NULL;

	page_mem = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, (u64)paddr, len);
	ASSERT (page_mem);

	return page_mem;
}

static void
vt_set_table_entry(u64 base_vaddr, uint index, u64 value){
	long *entry_vaddr;

	entry_vaddr = (long *)base_vaddr;
	entry_vaddr += index;
	*entry_vaddr = value;
}

static void
vt_get_table_entry(u64 base_vaddr, uint index, u64 *data){
	long *entry_vaddr;

	entry_vaddr = (long *)base_vaddr;
	entry_vaddr += index;

	*data = (u64)*entry_vaddr;
}

static bool
vt_check_page_flags(u64 entry)
{
	if (DT_IS_PCD(entry) | DT_IS_PWT(entry)){
		printf("[ERROR] (%s) ENTRY flags check failed. PCD or PWT=1. entry:%016llx\n", __func__, entry);
		return false;
	}	
	return true;

}

/**
 * Returns whether DEVIRT was requested.
 * True: DEVIRT was requested.
 * This function is called from vt_main.c
 */
bool
vt_devirt_requested()
{
	return devirt_grant;
}

/* Do VMXON
*/
static void
vt_vmxon (void)
{
	asm_vmxon (&currentcpu->vt.vmxon_region_phys);
}

static void
vt_vmxoff (void)
{
	ulong cr4;
	u8 port0x92;

	asm_rdcr4 (&cr4);
	if (!(cr4 & CR4_VMXE_BIT))
		return;
	/* enable A20 */
	/* the guest can set or clear A20M# because 
	   A20M# is ignored during VMX operation. however
	   A20M# is used after VMXOFF. */
	asm_inb (0x92, &port0x92);
	port0x92 |= 2;
	asm_outb (0x92, port0x92);
	asm_vmxoff ();
	cr4 &= ~CR4_VMXE_BIT;
	asm_wrcr4 (cr4);
}

/**
 * This function is called from vt_mainloop() in vt_main.c.
 * It is called only when vt_devirt_requested() is true.
 * It checks whether devirtualization can be triggered.
 * That is, CPL==0 && IF==0.
 */
bool
vt_devirtualizable ()
{

	if (!vt_devirt_requested()){
		DEBUG_PRINT(("[DEVIRT] (%s) Devirt is not requested. Just return.\n",
					__func__));
		return false;
	}

	ulong acr, dpl, rflags_if;

	// devirt_grant, CPL, IF bit check
	// Read DPL (=CPL).
	vt_read_sreg_acr (SREG_SS, &acr);
	dpl = acr & ACCESS_RIGHTS_DPL;
	dpl >>= 5;

	vt_vmptrld (current->u.vt.vi.vmcs_region_phys);
	vt_read_sreg_acr (SREG_SS, &acr);
	dpl = acr & ACCESS_RIGHTS_DPL;
	dpl >>= 5;
	
	// Read RFLAGS.
	vt_read_flags (&rflags_if);
	rflags_if &= RFLAGS_IF_BIT;

	if (dpl == 0 && rflags_if == 0){
		DEBUG_PRINT(("[DEVIRT] (%s) devirt_grant satisfied.\n", __func__));
		return true;
	} else {
		DEBUG_PRINT(("[DEVIRT] (%s) devirt_grant not satisfied.\n", __func__));
		DEBUG_PRINT(("[DEVIRT] (%s) SS.ACR:%lx, SS.ACR.DPL(bit 16,17):%lx, rflags.IF:%lx\n", 
					__func__,
					acr,
					dpl,
					rflags_if));
		return false;
	}
}

/**
 * It returns virtual address of start_address.
 */
long*
vt_memory_map_in_vmm (u64 start_address, u64 len)
{
	long *mem_long = NULL;
	mem_long = (long*)mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, start_address, len);
	ASSERT (mem_long);
	return mem_long;
}

/**
 * Map SHARED_AREA.
 */
static long *
vt_map_shared_area(void) 
{
	long *mem_long = NULL;
	mem_long = vt_memory_map_in_vmm (SHARED_AREA, SHARED_AREA_LEN);
	// initialize 0x48000000 ~ 0x4FFFFFFF to zero	
	memset ((void*)mem_long, '\0', SHARED_AREA_LEN);
	return mem_long;
}

// Static variables for reading of CPU states.
static ulong r15, r14, r13, r12, r11, r10, r9, r8, 				\
			   rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax,			\
			   gdtr_base, gdtr_limit, idtr_base, idtr_limit, 	\
			   cr2;
static u64 star_msr_value,				\
		   lstar_msr_value,				\
		   fmask_msr_value, 			\
		   kernel_gs_base_msr_value, 	\
		   sysenter_cs_msr_value,		\
		   sysenter_esp_msr_value, 		\
		   sysenter_eip_msr_value,		\
		   debugctl_msr_value,			\
		   fsbase_msr_value,			\
		   gsbase_msr_value, 			\
		   pat_msr_value,				\
		   perf_global_ctrl_msr_value,	\
		   bndcfgs_msr_value,			\
		   efer_msr_value;		
static u16 cs = 0, ss = 0, es = 0, ds = 0, fs = 0, gs = 0, tr = 0, ldtr = 0;
static u64 rip, cr8 = 0;

/**
 * Read host state to static variables.
 * It will be restored in REVIRT process.
 */
inline void
vt_read_host_state (void)
{
	// u64 feature_control_msr_value = 0; //for debug.
	u64 rip_value = 0;
	// There is no VMSAVE and VMLOAD in Intel. So we have to save that registers manually.
	// You can refer to a function: asm_vmresume_regs_64 in asm.s 
	// Check the list of registers stored in the function.
	// Additionally, VMRESUME saves some states. Please check which registers values are saved
	// in VMRESUME.
	
	// Read host state.
	asm_rdr15(&r15);
	asm_rdr14(&r14);
	asm_rdr13(&r13);
	asm_rdr12(&r12);
	asm_rdr11(&r11);
	asm_rdr10(&r10);
	asm_rdr9(&r9);
	asm_rdr8(&r8);

	asm_rdrdi(&rdi);
	asm_rdrsi(&rsi);
	asm_rdrbp(&rbp);
	asm_rdrsp(&rsp);
	asm_rdrbx(&rbx);
	asm_rdrdx(&rdx);
	asm_rdrcx(&rcx);

	// Debug registers might be useless. Keep them commented out.
	//asm_rddr7(&dr7);	// Saved in host_riv.
	//asm_rddr6(&dr6);

	asm_rdrax(&rax);

	asm_rdcr2(&cr2);
	asm_rdcr8(&cr8);
	asm_rdmsr64 (MSR_IA32_EFER, &efer_msr_value);

	asm_rdcs(&cs);
	asm_rdds(&ds);
	asm_rdes(&es);
	asm_rdss(&ss);

	asm_rdgdtr(&gdtr_base, &gdtr_limit);
	asm_rdidtr(&idtr_base, &idtr_limit);

	/** Folowing registers are saved with VMSAVE of SVM.
	 *
	 * STAR
	 * LSTAR
	 * CSTAR
	 * SFMASK
	 * KernelGSbase
	 * SYSENTER_CS
	 * SYSENTER_ESP
	 * SYSENTER_EIP
	 * FS
	 * GS
	 * LDTR
	 * TR
	 * 
	 * These states should be saved manually in Intel.
	 */
	// MSRs.
	asm_rdmsr64(MSR_IA32_SYSENTER_CS, &sysenter_cs_msr_value);
	asm_rdmsr64(MSR_IA32_SYSENTER_ESP, &sysenter_esp_msr_value);
	asm_rdmsr64(MSR_IA32_SYSENTER_EIP, &sysenter_eip_msr_value);

	// for debug.
	//asm_rdmsr64(MSR_IA32_FEATURE_CONTROL, &feature_control_msr_value);

	// Test Failed. Do not save it.
	//asm_rdmsr64(MSR_IA32_DEBUGCTL, &debugctl_msr_value);

	asm_rdmsr64(MSR_IA32_FS_BASE, &fsbase_msr_value);
	asm_rdmsr64(MSR_IA32_GS_BASE, &gsbase_msr_value);
	asm_rdmsr64(MSR_IA32_PAT, &pat_msr_value);

	// Do we need following MSRs? They might be the same as guests'. FIXME
	asm_rdmsr64(MSR_IA32_STAR, &star_msr_value);
	asm_rdmsr64(MSR_IA32_LSTAR, &lstar_msr_value);
	asm_rdmsr64(MSR_IA32_FMASK, &fmask_msr_value);
	asm_rdmsr64(MSR_IA32_KERNEL_GS_BASE, &kernel_gs_base_msr_value);

	// Test Failed.
	//asm_rdmsr64(MSR_IA32_PERF_GLOBAL_CTRL, &perf_global_ctrl_msr_value);
	// Test Failed.
	//asm_rdmsr64(MSR_IA32_BNDCFGS, &bndcfgs_msr_value);
	
	// SMBASE is expected not to be changed.
	// Additionally, this msr register can be read only in SMM.
	// We will not store SMBASE.
	//asm_rdmsr64(IA32_SMBASE, &smbase_msr_value);	

	asm_rdfs (&fs);
	asm_rdgs (&gs);
	asm_rdldtr (&ldtr);
	asm_rdtr (&tr);

	asm_rdrip((ulong *)&rip_value);
	rip = rip_value;

	DEBUG_PRINT(("[DEVIRT] (%s) RIP: %llx\n", __func__, rip));
}

/* Save u64 value to memory. */
static void
save_u64 (long *sa_base_vaddr, u64 sa_offset, u64 value)
{
	long *target_addr = 0;
	target_addr = sa_base_vaddr + sa_offset/sizeof(long*);
	*target_addr = value;
}

static struct regs_in_vmcs host_riv;	// riv: registers in vmcs.

/* Print read host state. */
static void
print_host_state (void)
{
	// Print saved host state.
	DEBUG_PRINT(("[DEVIRT] (%s) ******** Saved host state ********\n", __func__));

	// host_riv registers.
	DEBUG_PRINT_VAL(host_riv.cr0);
	DEBUG_PRINT_VAL(host_riv.cr3);
	DEBUG_PRINT_VAL(host_riv.cr4);
	DEBUG_PRINT_VAL(host_riv.cs.sel);
	DEBUG_PRINT_VAL(host_riv.cs.limit);
	DEBUG_PRINT_VAL(host_riv.cs.acr);
	DEBUG_PRINT_VAL(host_riv.cs.base);
	DEBUG_PRINT_VAL(host_riv.ds.sel);
	DEBUG_PRINT_VAL(host_riv.ds.limit);
	DEBUG_PRINT_VAL(host_riv.ds.acr);
	DEBUG_PRINT_VAL(host_riv.ds.base);
	DEBUG_PRINT_VAL(host_riv.es.sel);
	DEBUG_PRINT_VAL(host_riv.es.limit);
	DEBUG_PRINT_VAL(host_riv.es.acr);
	DEBUG_PRINT_VAL(host_riv.es.base);
	DEBUG_PRINT_VAL(host_riv.fs.sel);
	DEBUG_PRINT_VAL(host_riv.fs.limit);
	DEBUG_PRINT_VAL(host_riv.fs.acr);
	DEBUG_PRINT_VAL(host_riv.fs.base);
	DEBUG_PRINT_VAL(host_riv.gs.sel);
	DEBUG_PRINT_VAL(host_riv.gs.limit);
	DEBUG_PRINT_VAL(host_riv.gs.acr);
	DEBUG_PRINT_VAL(host_riv.gs.base);
	DEBUG_PRINT_VAL(host_riv.ss.sel);
	DEBUG_PRINT_VAL(host_riv.ss.limit);
	DEBUG_PRINT_VAL(host_riv.ss.acr);
	DEBUG_PRINT_VAL(host_riv.ss.base);
	DEBUG_PRINT_VAL(host_riv.ldtr.sel);
	DEBUG_PRINT_VAL(host_riv.ldtr.limit);
	DEBUG_PRINT_VAL(host_riv.ldtr.acr);
	DEBUG_PRINT_VAL(host_riv.ldtr.base);
	DEBUG_PRINT_VAL(host_riv.tr.sel);
	DEBUG_PRINT_VAL(host_riv.tr.limit);
	DEBUG_PRINT_VAL(host_riv.tr.acr);
	DEBUG_PRINT_VAL(host_riv.tr.base);
	DEBUG_PRINT_VAL(host_riv.gdtr.limit);
	DEBUG_PRINT_VAL(host_riv.gdtr.base);
	DEBUG_PRINT_VAL(host_riv.idtr.limit);
	DEBUG_PRINT_VAL(host_riv.idtr.base);
	DEBUG_PRINT_VAL(host_riv.dr7);
	DEBUG_PRINT_VAL(host_riv.rflags);

	// General purpose registers.
	DEBUG_PRINT_VAL(r15);
	DEBUG_PRINT_VAL(r14);
	DEBUG_PRINT_VAL(r13);
	DEBUG_PRINT_VAL(r12);
	DEBUG_PRINT_VAL(r11);
	DEBUG_PRINT_VAL(r10);
	DEBUG_PRINT_VAL(r9);
	DEBUG_PRINT_VAL(r8);
	DEBUG_PRINT_VAL(rdi);
	DEBUG_PRINT_VAL(rsi);
	DEBUG_PRINT_VAL(rbp);
	DEBUG_PRINT_VAL(rsp);
	DEBUG_PRINT_VAL(rbx);
	DEBUG_PRINT_VAL(rdx);
	DEBUG_PRINT_VAL(rcx);
	DEBUG_PRINT_VAL(rax);
	DEBUG_PRINT_VAL(rsp);
	DEBUG_PRINT_VAL(rip);
	DEBUG_PRINT_VAL(cr8);

	// MSRs
	DEBUG_PRINT_VAL(sysenter_cs_msr_value);
	DEBUG_PRINT_VAL(sysenter_esp_msr_value);
	DEBUG_PRINT_VAL(sysenter_eip_msr_value);
	DEBUG_PRINT_VAL(efer_msr_value);
	DEBUG_PRINT_VAL(pat_msr_value);
	DEBUG_PRINT_VAL(star_msr_value);
	DEBUG_PRINT_VAL(lstar_msr_value);
	DEBUG_PRINT_VAL(fmask_msr_value);
	DEBUG_PRINT_VAL(fsbase_msr_value);
	DEBUG_PRINT_VAL(gsbase_msr_value);
	DEBUG_PRINT_VAL(kernel_gs_base_msr_value);
	DEBUG_PRINT_VAL(debugctl_msr_value);
	DEBUG_PRINT_VAL(perf_global_ctrl_msr_value);
	DEBUG_PRINT_VAL(bndcfgs_msr_value);

	// Physical address of VMCS pointer 
	DEBUG_PRINT(("[DEVIRT] (%s) *** Saved VMCS pointer ***\n", __func__));
	DEBUG_PRINT(("[DEVIRT] (%s) VMCS ptr paddr:%llx\n", 
				__func__, current->u.vt.vi.vmcs_region_phys));

	DEBUG_PRINT(("[DEVIRT] (%s) $$$$$$$ Saved host state END $$$$$$$\n", __func__));
}

static long * sa_hriv = NULL;	// SHARED_AREA_VT_HRIV
static long * sa_misc = NULL;	// SHARED_AREA_VT_MISC
/**
 * Save host state to SHARED_AREA.
 */
void
vt_save_host_state (long *shared_area_vaddr)
{
	long *sa_hriv_vaddr = NULL;	// Save registers related to VMCS.

	//Save host state.
	/**
	 * We are not going to use "VMCS host state area" for storing
	 * current host state. Because VT sets VMCS host state area 
	 * only at the initialization of VMCS.
	 * Although we can use the area and re-initialize it,
	 * we decided to use SHARED_AREA for storing current host state.
	 */
	sa_hriv_vaddr = shared_area_vaddr + ((u64)SHARED_AREA_VT_OFFSET_HRIV)/sizeof(long*);

	// Get host states (registers in VMCS) using vt_get_current_regs_in_vmcs() function.
	vt_get_current_regs_in_vmcs (&host_riv);

	// Save host states (registers in VMCS) to the shared area.
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_ES_SEL, host_riv.es.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_ES_LIMIT, host_riv.es.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_ES_ACCESS_RIGHT, host_riv.es.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_ES_BASE, host_riv.es.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CS_SEL, host_riv.cs.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CS_LIMIT, host_riv.cs.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CS_ACCESS_RIGHT, host_riv.cs.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CS_BASE, host_riv.cs.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_SS_SEL, host_riv.ss.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_SS_LIMIT, host_riv.ss.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_SS_ACCESS_RIGHT, host_riv.ss.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_SS_BASE, host_riv.ss.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_DS_SEL, host_riv.ds.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_DS_LIMIT, host_riv.ds.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_DS_ACCESS_RIGHT, host_riv.ds.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_DS_BASE, host_riv.ds.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_FS_SEL, host_riv.fs.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_FS_LIMIT, host_riv.fs.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_FS_ACCESS_RIGHT, host_riv.fs.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_FS_BASE, host_riv.fs.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GS_SEL, host_riv.gs.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GS_LIMIT, host_riv.gs.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GS_ACCESS_RIGHT, host_riv.gs.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GS_BASE, host_riv.gs.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_LDTR_SEL, host_riv.ldtr.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_LDTR_LIMIT, host_riv.ldtr.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_LDTR_ACCESS_RIGHT, host_riv.ldtr.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_LDTR_BASE, host_riv.ldtr.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_TR_SEL, host_riv.tr.sel);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_TR_LIMIT, host_riv.tr.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_TR_ACCESS_RIGHT, host_riv.tr.acr);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_TR_BASE, host_riv.tr.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GDTR_LIMIT, host_riv.gdtr.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_GDTR_BASE, host_riv.gdtr.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_IDTR_LIMIT, host_riv.idtr.limit);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_IDTR_BASE, host_riv.idtr.base);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CR0, host_riv.cr0);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CR3, host_riv.cr3);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_CR4, host_riv.cr4);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_DR7, host_riv.dr7);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_HRIV_RFLAGS, host_riv.rflags);

	// Save Host's virtual addresses for HRIV and MISC area.
	// Two mappings should be unmapped after revirt.
	sa_hriv = (long *)vt_mapmem_page(SHARED_AREA_VT_HRIV, 0x500);
	sa_misc = (long *)vt_mapmem_page(SHARED_AREA_VT_MISC, 0x500);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_HOST_HRIV_VADDR, sa_hriv);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_HOST_MISC_VADDR, sa_misc);

	DEBUG_PRINT_VAL(sa_hriv);
	DEBUG_PRINT_VAL(sa_misc);

	// Save registers.
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R15, r15);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R14, r14);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R13, r13);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R12, r12);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R11, r11);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R10, r10);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R9,  r9);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_R8,  r8);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RDI, rdi);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RSI, rsi);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RBP, rbp);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RSP, rsp);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RBX, rbx);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RDX, rdx);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RCX, rcx);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RAX, rax);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_RIP, rip);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_CR2, cr2);
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_CR8, cr8);

	// MSRs
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_SYSENTER_CS, sysenter_cs_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_SYSENTER_ESP, sysenter_esp_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_SYSENTER_EIP, sysenter_eip_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_EFER, efer_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_PAT, pat_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_STAR, star_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_LSTAR, lstar_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_FMASK, fmask_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_FS_BASE, fsbase_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_GS_BASE, gsbase_msr_value);
	save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_KERNEL_GS_BASE, kernel_gs_base_msr_value);

	// we didn't read these 3 values
	//save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_DEBUGCTL, debugctl_msr_value);
	//save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_PERF_GLOBAL, perf_global_ctrl_msr_value);
	//save_u64(shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_BNDCFGS, bndcfgs_msr_value);*/

	/* SMBASE is expected not to be changed.
	 * Additionally, this msr register can be read only in SMM.
	 * We will not store SMBASE.
	 */
	// SMBASE	

	// Save physical address of VMCS ptr.
	save_u64 (shared_area_vaddr, SHARED_AREA_VT_OFFSET_MISC_VMCS_PTR_PA, 
			current->u.vt.vi.vmcs_region_phys);		// Need to be checked. TODO.

	// print.
	print_host_state();
}

static struct regs_in_vmcs guest_riv;
static ulong grsp, grip,		\
		  grax, grbx, grcx, grdx, grdi, grsi, grbp,			\
		  gr8, gr9, gr10, gr11, gr12, gr13, gr14, gr15,		\
		  gcr2;
//static ulong  gcr8;
static u64 gefer, gsysenter_cs, gsysenter_esp, gsysenter_eip,	\
		gdebugctl, gpat, gperf_global, gbndcfgs;

/* Print read guest state. (Print static variables) */
static void
print_read_guest_state (void)
{
	DEBUG_PRINT(("[DEVIRT] (%s) ******* Read Guest State *******\n", __func__));
	DEBUG_PRINT_VAL(guest_riv.cr0);
	DEBUG_PRINT_VAL(guest_riv.cr3);
	DEBUG_PRINT_VAL(guest_riv.cr4);
	DEBUG_PRINT_VAL(guest_riv.rflags);

	DEBUG_PRINT_VAL(guest_riv.cs.sel);
	DEBUG_PRINT_VAL(guest_riv.ds.sel);
	DEBUG_PRINT_VAL(guest_riv.es.sel);
	DEBUG_PRINT_VAL(guest_riv.fs.sel);
	DEBUG_PRINT_VAL(guest_riv.gs.sel);
	DEBUG_PRINT_VAL(guest_riv.ss.sel);
	DEBUG_PRINT_VAL(guest_riv.tr.sel);
	DEBUG_PRINT_VAL(guest_riv.ldtr.sel);

	DEBUG_PRINT_VAL(guest_riv.cs.acr);
	DEBUG_PRINT_VAL(guest_riv.ds.acr);
	DEBUG_PRINT_VAL(guest_riv.es.acr);
	DEBUG_PRINT_VAL(guest_riv.fs.acr);
	DEBUG_PRINT_VAL(guest_riv.gs.acr);
	DEBUG_PRINT_VAL(guest_riv.ss.acr);
	DEBUG_PRINT_VAL(guest_riv.tr.acr);
	DEBUG_PRINT_VAL(guest_riv.ldtr.acr);

	DEBUG_PRINT_VAL(guest_riv.cs.limit);
	DEBUG_PRINT_VAL(guest_riv.ds.limit);
	DEBUG_PRINT_VAL(guest_riv.es.limit);
	DEBUG_PRINT_VAL(guest_riv.fs.limit);
	DEBUG_PRINT_VAL(guest_riv.gs.limit);
	DEBUG_PRINT_VAL(guest_riv.ss.limit);
	DEBUG_PRINT_VAL(guest_riv.tr.limit);
	DEBUG_PRINT_VAL(guest_riv.ldtr.limit);

	DEBUG_PRINT_VAL(guest_riv.cs.base);
	DEBUG_PRINT_VAL(guest_riv.ds.base);
	DEBUG_PRINT_VAL(guest_riv.es.base);
	DEBUG_PRINT_VAL(guest_riv.fs.base);
	DEBUG_PRINT_VAL(guest_riv.gs.base);
	DEBUG_PRINT_VAL(guest_riv.ss.base);
	DEBUG_PRINT_VAL(guest_riv.tr.base);
	DEBUG_PRINT_VAL(guest_riv.ldtr.base);

	DEBUG_PRINT_VAL(guest_riv.gdtr.limit);
	DEBUG_PRINT_VAL(guest_riv.idtr.limit);
	DEBUG_PRINT_VAL(guest_riv.gdtr.base);
	DEBUG_PRINT_VAL(guest_riv.idtr.base);

	DEBUG_PRINT(("[DEVIRT] (%s) $$$$$$$ Read Guest State END $$$$$$$\n", __func__));
}

/* Print saved guest state. (Print values in memory) */
static void
print_saved_guest_state (long *mem_long, short *mem_short)
{
	DEBUG_PRINT(("[DEVIRT] (%s) ******* Saved Guest State *******\n", __func__));

	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RSP, "g rsp");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RFLAGS, "g rflags");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RIP, "g rip");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CR0, "g cr0");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CR3, "g cr3");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CR4, "g cr4");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DR7, "g dr7");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_EFER, "g efer");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_CS, "g sysenter_cs");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_ESP, "g sysenter_esp");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_EIP, "g sysenter_eip");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DEBUGCTL, "g debugctl");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_PAT, "g pat");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_PERF_GLOBAL, "g perf_global");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_BNDCFGS, "g bndcfgs");

	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CS_SEL, "g cs_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CS_ACCESS_RIGHT, "g cs_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CS_LIMIT, "g cs_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CS_BASE, "g cs_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DS_SEL, "g ds_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DS_ACCESS_RIGHT, "g ds_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DS_LIMIT, "g ds_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_DS_BASE, "g ds_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_ES_SEL, "g es_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_ES_ACCESS_RIGHT, "g es_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_ES_LIMIT, "g es_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_ES_BASE, "g es_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_FS_SEL, "g fs_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_FS_ACCESS_RIGHT, "g fs_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_FS_LIMIT, "g fs_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_FS_BASE, "g fs_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_GS_SEL, "g gs_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_GS_ACCESS_RIGHT, "g gs_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_GS_LIMIT, "g gs_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_GS_BASE, "g gs_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SS_SEL, "g ss_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SS_ACCESS_RIGHT, "g ss_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SS_LIMIT, "g ss_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_SS_BASE, "g ss_base");
	vt_print_val (__func__, mem_short+ SHARED_AREA_VT_OFFSET_GDTR_LIMIT, "g gdtr_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_GDTR_BASE, "g gdtr_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_LDTR_SEL, "g ldtr_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_LDTR_ACCESS_RIGHT, "g ldtr_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_LDTR_LIMIT, "g ldtr_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_LDTR_BASE, "g ldtr_base");
	vt_print_val (__func__, mem_short+ SHARED_AREA_VT_OFFSET_IDTR_LIMIT, "g idtr_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_IDTR_BASE, "g idtr_base");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_TR_SEL, "g tr_sel");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_TR_ACCESS_RIGHT, "g tr_ar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_TR_LIMIT, "g tr_limit");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_TR_BASE, "g tr_base");
	
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RAX, "g rax");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RBX, "g rbx");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RCX, "g rcx");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RDX, "g rdx");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RDI, "g rdi");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RSI, "g rsi");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_RBP, "g rbp");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R8 , "g r8");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R9 , "g r9");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R10, "g r10");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R11, "g r11");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R12, "g r12");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R13, "g r13");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R14, "g r14");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_R15, "g r15");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_CR2, "g cr2");

	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_STAR, "g star");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_LSTAR, "g lstar");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_FMASK, "g fmask");
	vt_print_val (__func__, mem_long + SHARED_AREA_VT_OFFSET_KERNEL_GSBASE, "g kernel_gsbase");

	DEBUG_PRINT(("[DEVIRT] (%s) $$$$$$$ Saved Guest State END $$$$$$$\n", __func__));
}


/**
 * Save guest state to SHARED_AREA.
 */
static void
vt_save_guest_state (long *shared_area_vaddr)
{
	long *mem_long = NULL;
	short *mem_short = NULL;
	//u64 gfeature_control;	// For debug. 
	//u64 gvmx_cr0_fixed0;	// For debug. 
	//u64 gvmx_cr0_fixed1;	// For debug. 
	//u64 gvmx_cr4_fixed0;	// For debug. 
	//u64 gvmx_cr4_fixed1;	// For debug. 

	//ulong temp_proc_based_clt1;	// For debug.
	//ulong temp_proc_based_clt2;	// For debug.
	//ulong temp_vmexit_ctrl;		// For debug.
	//ulong temp_vmentry_ctrl;		// For debug.
	//ulong temp_exit_store_count;	// For debug.
	//ulong temp_exit_load_count;	// For debug.
	//ulong temp_entry_load_count;	// For debug.
	//ulong *temp_msr;				// For debug.
	//int temp_i = 0;				// For debug.


	mem_long = shared_area_vaddr + SHARED_AREA_VT_OFFSET_GUEST_STATE/8;
	mem_short = (short*)shared_area_vaddr;

	DEBUG_PRINT(("[DEVIRT] (%s) mem_long:%llx\n", __func__, (u64)mem_long));
	DEBUG_PRINT(("[DEVIRT] (%s) mapmem result mem:%llx\n", __func__, (u64)shared_area_vaddr));

	/** READ GUEST STATE **/
	// Load VMCS ptr.	// What is the original value?? Need to check! FIXME
	vt_vmptrld (current->u.vt.vi.vmcs_region_phys);

	// Read guest area of VMCS.
	vt_get_vmcs_regs_in_vmcs (&guest_riv);

	// Read other state.
	vt_read_general_reg (GENERAL_REG_RSP, &grsp);
	vt_read_ip (&grip);
	vt_read_msr (MSR_IA32_EFER, &gefer);
	vt_read_msr (MSR_IA32_SYSENTER_CS, &gsysenter_cs);
	vt_read_msr (MSR_IA32_SYSENTER_ESP, &gsysenter_esp);
	vt_read_msr (MSR_IA32_SYSENTER_EIP, &gsysenter_eip);
	vt_read_msr (MSR_IA32_DEBUGCTL, &gdebugctl);
	vt_read_msr (MSR_IA32_PAT, &gpat);
	vt_read_msr (MSR_IA32_PERF_GLOBAL_CTRL, &gperf_global);
	vt_read_msr (MSR_IA32_BNDCFGS, &gbndcfgs);
	//vt_read_msr (MSR_IA32_SMBASE, &gsmbase);
	vt_read_general_reg (GENERAL_REG_RAX, &grax);
	vt_read_general_reg (GENERAL_REG_RBX, &grbx);
	vt_read_general_reg (GENERAL_REG_RCX, &grcx);
	vt_read_general_reg (GENERAL_REG_RDX, &grdx);
	vt_read_general_reg (GENERAL_REG_RDI, &grdi);
	vt_read_general_reg (GENERAL_REG_RSI, &grsi);
	vt_read_general_reg (GENERAL_REG_RBP, &grbp);
	vt_read_general_reg (GENERAL_REG_R8, &gr8);
	vt_read_general_reg (GENERAL_REG_R9, &gr9);
	vt_read_general_reg (GENERAL_REG_R10, &gr10);
	vt_read_general_reg (GENERAL_REG_R11, &gr11);
	vt_read_general_reg (GENERAL_REG_R12, &gr12);
	vt_read_general_reg (GENERAL_REG_R13, &gr13);
	vt_read_general_reg (GENERAL_REG_R14, &gr14);
	vt_read_general_reg (GENERAL_REG_R15, &gr15);

	vt_read_control_reg (CONTROL_REG_CR2, &gcr2);
	//vt_read_control_reg (CONTROL_REG_CR8, &gcr8);
	
	// Print read guest state.
	print_read_guest_state ();

	/* Printing for debug. */
	//vt_read_msr (MSR_IA32_FEATURE_CONTROL, &gfeature_control);
	//vt_read_msr (MSR_IA32_VMX_CR0_FIXED0, &gvmx_cr0_fixed0);
	//vt_read_msr (MSR_IA32_VMX_CR0_FIXED1, &gvmx_cr0_fixed1);
	//vt_read_msr (MSR_IA32_VMX_CR4_FIXED0, &gvmx_cr4_fixed0);
	//vt_read_msr (MSR_IA32_VMX_CR4_FIXED1, &gvmx_cr4_fixed1);
	//asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &temp_proc_based_clt1);
	//asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &temp_proc_based_clt2);
	//asm_vmread (VMCS_VMEXIT_CTL, &temp_vmexit_ctrl);
	//asm_vmread (VMCS_VMENTRY_CTL, &temp_vmentry_ctrl);
	//asm_vmread (VMCS_VMEXIT_MSR_STORE_COUNT, &temp_exit_store_count);
	//asm_vmread (VMCS_VMEXIT_MSR_LOAD_COUNT, &temp_exit_load_count);
	//asm_vmread (VMCS_VMENTRY_MSR_LOAD_COUNT, &temp_entry_load_count);

	//DEBUG_PRINT(("[DEVIRT] (%s) gfeature_control:%llx\n", __func__, gfeature_control));
	//DEBUG_PRINT(("[DEVIRT] (%s) gvmx_cr0_fixed0:%llx\n", __func__, gvmx_cr0_fixed0));
	//DEBUG_PRINT(("[DEVIRT] (%s) gvmx_cr0_fixed1:%llx\n", __func__, gvmx_cr0_fixed1));
	//DEBUG_PRINT(("[DEVIRT] (%s) gvmx_cr4_fixed0:%llx\n", __func__, gvmx_cr4_fixed0));
	//DEBUG_PRINT(("[DEVIRT] (%s) gvmx_cr4_fixed1:%llx\n", __func__, gvmx_cr4_fixed1));
	//DEBUG_PRINT(("[DEVIRT] (%s) proc_based_clt1:%lx\n", __func__, temp_proc_based_clt1));
	//DEBUG_PRINT(("[DEVIRT] (%s) proc_based_clt2:%lx\n", __func__, temp_proc_based_clt2));
	//DEBUG_PRINT(("[DEVIRT] (%s) vmexit_ctl:%lx\n", __func__, temp_vmexit_ctrl));
	//DEBUG_PRINT(("[DEVIRT] (%s) vmentry_ctl:%lx\n", __func__, temp_vmentry_ctrl));
	//DEBUG_PRINT(("[DEVIRT] (%s) temp_exit_store_count:%lx\n", __func__, temp_exit_store_count));
	//DEBUG_PRINT(("[DEVIRT] (%s) temp_exit_load_count:%lx\n", __func__, temp_exit_load_count));
	//DEBUG_PRINT(("[DEVIRT] (%s) temp_entry_load_count:%lx\n", __func__, temp_entry_load_count));
	
	//DEBUG_PRINT(("[DEVIRT] (%s) current->u.vt.msr.vmm:%lx\n", __func__, current->u.vt.msr.vmm));
	//DEBUG_PRINT(("[DEVIRT] (%s) current->u.vt.msr.guest:%lx\n", __func__, current->u.vt.msr.guest));
	//DEBUG_PRINT(("[DEVIRT] (%s) current->u.vt.msr.count:%lx\n", __func__, current->u.vt.msr.count));

	/*DEBUG_PRINT(("[DEVIRT] (%s) ***PRINT VMM MSR***\n", __func__));
	temp_msr = current->u.vt.msr.vmm; 
	for (temp_i = 0; temp_i < current->u.vt.msr.count; temp_i++){
		DEBUG_PRINT(("[DEVIRT] (%s) msr (%d):%lx", __func__, temp_i, *temp_msr));
		temp_msr++;
		DEBUG_PRINT(("\t %lx\n", *temp_msr));
		temp_msr++;
	}

	DEBUG_PRINT(("[DEVIRT] (%s) ***PRINT GUEST MSR***\n", __func__));
	temp_msr = current->u.vt.msr.guest; 
	for (temp_i = 0; temp_i < current->u.vt.msr.count; temp_i++){
		DEBUG_PRINT(("[DEVIRT] (%s) msr (%d):%lx", __func__,temp_i, *temp_msr));
		temp_msr++;
		DEBUG_PRINT(("\t %lx\n", *temp_msr));
		temp_msr++;
	}*/


	/** MODIFY GUEST STATE **/
	gefer = 0xd01;	// EFER hard coding.
	guest_riv.cr4 &= ~CR4_VMXE_BIT;	// Unset gVMXE because of VMXOFF.

	/** SAVE GUEST STATE **/
	// Guest state area in VMCS.
	*(mem_long + SHARED_AREA_VT_OFFSET_RSP			) = grsp;
	*(mem_long + SHARED_AREA_VT_OFFSET_RFLAGS		) = guest_riv.rflags;
	*(mem_long + SHARED_AREA_VT_OFFSET_RIP			) = grip;
	*(mem_long + SHARED_AREA_VT_OFFSET_CR0			) = guest_riv.cr0;
	*(mem_long + SHARED_AREA_VT_OFFSET_CR3			) = guest_riv.cr3;
	*(mem_long + SHARED_AREA_VT_OFFSET_CR4			) = guest_riv.cr4;
	*(mem_long + SHARED_AREA_VT_OFFSET_DR7			) = guest_riv.dr7;
	*(mem_long + SHARED_AREA_VT_OFFSET_EFER			) = gefer;
	*(mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_CS	) = gsysenter_cs;
	*(mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_ESP	) = gsysenter_esp;
	*(mem_long + SHARED_AREA_VT_OFFSET_SYSENTER_EIP	) = gsysenter_eip;
	*(mem_long + SHARED_AREA_VT_OFFSET_DEBUGCTL		) = gdebugctl;
	*(mem_long + SHARED_AREA_VT_OFFSET_PAT			) = gpat;
	*(mem_long + SHARED_AREA_VT_OFFSET_PERF_GLOBAL	) = gperf_global;
	*(mem_long + SHARED_AREA_VT_OFFSET_BNDCFGS		) = gbndcfgs;
	// Do not save SMBASE. Assuming it is not changed.
	//smbase
	// Segment Registers
	*(mem_long + SHARED_AREA_VT_OFFSET_CS_SEL		) = guest_riv.cs.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_CS_ACCESS_RIGHT		) = guest_riv.cs.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_CS_LIMIT		) = guest_riv.cs.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_CS_BASE		) = guest_riv.cs.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_DS_SEL		) = guest_riv.ds.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_DS_ACCESS_RIGHT		) = guest_riv.ds.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_DS_LIMIT		) = guest_riv.ds.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_DS_BASE		) = guest_riv.ds.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_ES_SEL		) = guest_riv.es.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_ES_ACCESS_RIGHT		) = guest_riv.es.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_ES_LIMIT		) = guest_riv.es.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_ES_BASE		) = guest_riv.es.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_FS_SEL		) = guest_riv.fs.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_FS_ACCESS_RIGHT		) = guest_riv.fs.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_FS_LIMIT		) = guest_riv.fs.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_FS_BASE		) = guest_riv.fs.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_GS_SEL		) = guest_riv.gs.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_GS_ACCESS_RIGHT		) = guest_riv.gs.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_GS_LIMIT		) = guest_riv.gs.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_GS_BASE		) = guest_riv.gs.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_SS_SEL		) = guest_riv.ss.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_SS_ACCESS_RIGHT		) = guest_riv.ss.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_SS_LIMIT		) = guest_riv.ss.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_SS_BASE		) = guest_riv.ss.base;
	*(mem_short+ SHARED_AREA_VT_OFFSET_GDTR_LIMIT	) = guest_riv.gdtr.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_GDTR_BASE	) = guest_riv.gdtr.base;	
	*(mem_long + SHARED_AREA_VT_OFFSET_LDTR_SEL		) = guest_riv.ldtr.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_LDTR_ACCESS_RIGHT	) = guest_riv.ldtr.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_LDTR_LIMIT	) = guest_riv.ldtr.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_LDTR_BASE	) = guest_riv.ldtr.base;
	*(mem_short+ SHARED_AREA_VT_OFFSET_IDTR_LIMIT	) = guest_riv.idtr.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_IDTR_BASE	) = guest_riv.idtr.base;
	*(mem_long + SHARED_AREA_VT_OFFSET_TR_SEL		) = guest_riv.tr.sel;
	*(mem_long + SHARED_AREA_VT_OFFSET_TR_ACCESS_RIGHT		) = guest_riv.tr.acr;
	*(mem_long + SHARED_AREA_VT_OFFSET_TR_LIMIT		) = guest_riv.tr.limit;
	*(mem_long + SHARED_AREA_VT_OFFSET_TR_BASE		) = guest_riv.tr.base;
	
	// The other vcpu registers.
	*(mem_long + SHARED_AREA_VT_OFFSET_RAX			) = grax;
	*(mem_long + SHARED_AREA_VT_OFFSET_RBX			) = grbx;
	*(mem_long + SHARED_AREA_VT_OFFSET_RCX			) = grcx;
	*(mem_long + SHARED_AREA_VT_OFFSET_RDX			) = grdx;
	*(mem_long + SHARED_AREA_VT_OFFSET_RDI			) = grdi;
	*(mem_long + SHARED_AREA_VT_OFFSET_RSI			) = grsi;
	*(mem_long + SHARED_AREA_VT_OFFSET_RBP			) = grbp;
	*(mem_long + SHARED_AREA_VT_OFFSET_R8			) = gr8;
	*(mem_long + SHARED_AREA_VT_OFFSET_R9			) = gr9;
	*(mem_long + SHARED_AREA_VT_OFFSET_R10			) = gr10;
	*(mem_long + SHARED_AREA_VT_OFFSET_R11			) = gr11;
	*(mem_long + SHARED_AREA_VT_OFFSET_R12			) = gr12;
	*(mem_long + SHARED_AREA_VT_OFFSET_R13			) = gr13;
	*(mem_long + SHARED_AREA_VT_OFFSET_R14			) = gr14;
	*(mem_long + SHARED_AREA_VT_OFFSET_R15			) = gr15;
	*(mem_long + SHARED_AREA_VT_OFFSET_CR2			) = gcr2;
	//*(mem_long + SHARED_AREA_VT_OFFSET_CR8			) = gcr8;	// Do not save.
	
	*(mem_long + SHARED_AREA_VT_OFFSET_STAR			) = current->u.vt.msr.star;
	*(mem_long + SHARED_AREA_VT_OFFSET_LSTAR		) = current->u.vt.msr.lstar;
	*(mem_long + SHARED_AREA_VT_OFFSET_FMASK		) = current->u.vt.msr.fmask;
	*(mem_long + SHARED_AREA_VT_OFFSET_KERNEL_GSBASE) = current->u.vt.msr.kerngs;

	// Print saved guest state.
	print_saved_guest_state (mem_long, mem_short);
}

/**
 * Find empty entries of the same address for both guest and host pml4 tables.
 * Returns index of the found entries.
 */
static int
vt_get_empty_entry_idx (long* g_page_table_base_address, long* h_page_table_base_address)
{
	int i = 0;
	bool empty_entry_found = false;

	for (i=0; i<512; i++)
	{
		if (*(g_page_table_base_address + i) == 0  && *(h_page_table_base_address + i) == 0 ) {
			DEBUG_PRINT(("[DEVIRT] (%s) empty entry: g: %lx, h: %lx\n", __func__, 
						*(g_page_table_base_address + i), *(h_page_table_base_address + i)));
			DEBUG_PRINT(("[DEVIRT] (%s) empty entry number: %d\n", __func__, i));
			empty_entry_found = true;
			break;
		}
	}

	if (empty_entry_found) {
		return i;
	} else {
		return -1;
	}
}


/**
 * Allocate a lower-level table to the designated entry.
 * "alloc_address" is the offset from the SHARED_AREA_VT_PAGE_TABLE.
 */
static long*
vt_alloc_table_to_entry (long* page_table_entry, u64 alloc_address)
{	
	alloc_address += SHARED_AREA_VT_PAGE_TABLE_AREA;
	*page_table_entry = alloc_address | PTE_P_BIT | PTE_RW_BIT;	// No US bit.
	return (long*)alloc_address;
}


/** For Debugging. INVVPID, INVEPT.
 */
static bool
ept_enabled (void)
{
	return (current->u.vt.vr.pg || current->u.vt.unrestricted_guest)
		&& current->u.vt.ept;
}
void
vt_flush_guest_tlb (void)
{
	struct invvpid_desc desc;
	struct invept_desc eptdesc;
	ulong vpid;

	vpid = current->u.vt.vpid;
	if (vpid) {
		desc.vpid = vpid;
		asm_invvpid (INVVPID_TYPE_ALL_CONTEXTS, &desc);
	}
	if (ept_enabled () && current->u.vt.invept_available) {
		eptdesc.reserved = 0;
		asm_invept (INVEPT_TYPE_ALL_CONTEXTS, &eptdesc);
	}
}


void
vt_prepare_resume (void)
{
	if (!current->u.vt.saved_vmcs){
		DEBUG_PRINT (("[DEVIRT] (%s) alloc new page\n", __func__));
		alloc_page (&current->u.vt.saved_vmcs, NULL);
	}
	asm_vmclear (&current->u.vt.vi.vmcs_region_phys);
	memcpy (current->u.vt.saved_vmcs, current->u.vt.vi.vmcs_region_virt,
		PAGESIZE);
	asm_vmptrld (&current->u.vt.vi.vmcs_region_phys);
	current->u.vt.first = true;

	/* TODO We need to consider locking mechanism when
	 * it is run on multi-core system.
	 */
	//spinlock_lock (&currentcpu->suspend_lock);
}

/**
 * The control is handed over to Guest OS ("handover")
 * and VMM(BitVisor) becomes disabled.
 *
 * Do following jobs.
 * 1. Set page tables for DEVIRT.
 * 2. Preparation for REVIRT.
 * 3. Run switch code and devirtualized.
 */
void inline
vt_devirt_handover (long *shared_area_vaddr)
{
	ulong hcr3, gcr3;
	void *g_pml4_table_addr = NULL;
	void *h_pml4_table_addr = NULL;
	long * g_page_mem = NULL;
	long * h_page_mem = NULL;
	long i = 0;
	long *gcr3_bak = NULL;
	long *empty_entry_idx_bak = NULL;
	long *g_pml4_empty_entry = NULL;
	long *h_pml4_empty_entry = NULL;
	long *g_pdp_alloc_addr = NULL;	// For DEBUG print
	long *g_pd_alloc_addr = NULL;	// For DEBUG print
	long *g_pt_alloc_addr = NULL;	// For DEBUG print
	long *h_pdp_alloc_addr = NULL;	// For DEBUG print
	int level3_entry_num = 0;
	int level2_entry_num = 0;
	int level1_entry_num = 0;
	int switch_code_size = 0;
	
	char switch_code[] = { 
		0x0f, 0x01, 0x97, 0x46, 0x01, 0x00,
		0x00, 0x0f, 0x01, 0x9f, 0x76, 0x01, 0x00, 0x00, 0x4c, 0x8b, 0x57, 0x08,
		0x49, 0x81, 0xe2, 0xff, 0xfd, 0xff, 0xff, 0x49, 0x81, 0xca, 0x00, 0x00,
		0x20, 0x00, 0x41, 0x52, 0x9d, 0x0f, 0x22, 0xda, 0x8e, 0xa7, 0xe0, 0x00,
		0x00, 0x00, 0x8e, 0xaf, 0x00, 0x01, 0x00, 0x00, 0xb9, 0x00, 0x01, 0x00,
		0xc0, 0x8b, 0x87, 0xf8, 0x00, 0x00, 0x00, 0x8b, 0x97, 0xfc, 0x00, 0x00,
		0x00, 0x0f, 0x30, 0xb9, 0x01, 0x01, 0x00, 0xc0, 0x8b, 0x87, 0x18, 0x01,
		0x00, 0x00, 0x8b, 0x97, 0x1c, 0x01, 0x00, 0x00, 0x0f, 0x30, 0xb9, 0x74,
		0x01, 0x00, 0x00, 0x8b, 0x47, 0x40, 0x8b, 0x57, 0x44, 0x0f, 0x30, 0xb9,
		0x75, 0x01, 0x00, 0x00, 0x8b, 0x47, 0x48, 0x8b, 0x57, 0x4c, 0x0f, 0x30,
		0xb9, 0x76, 0x01, 0x00, 0x00, 0x8b, 0x47, 0x50, 0x8b, 0x57, 0x54, 0x0f,
		0x30, 0x48, 0x8b, 0x5f, 0x30, 0x0f, 0x23, 0xfb, 0xb9, 0x80, 0x00, 0x00,
		0xc0, 0x8b, 0x47, 0x38, 0x8b, 0x57, 0x3c, 0x0f, 0x30, 0x48, 0x8b, 0x5f,
		0x18, 0x0f, 0x22, 0xc3, 0x48, 0x8b, 0x5f, 0x28, 0x0f, 0x22, 0xe3, 0x48,
		0x8b, 0x9f, 0x18, 0x02, 0x00, 0x00, 0x0f, 0x22, 0xd3, 0x8e, 0x9f, 0xa0,
		0x00, 0x00, 0x00, 0x8e, 0x87, 0xc0, 0x00, 0x00, 0x00, 0x48, 0xc7, 0xc2,
		0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0x97, 0x48, 0x02, 0x00, 0x00, 0x48,
		0x89, 0x97, 0x50, 0x02, 0x00, 0x00, 0x48, 0x0f, 0xb2, 0x97, 0x48, 0x02,
		0x00, 0x00, 0x48, 0x8b, 0x8f, 0x48, 0x01, 0x00, 0x00, 0x48, 0x03, 0x8f,
		0x80, 0x01, 0x00, 0x00, 0x8b, 0x41, 0x04, 0x89, 0xc3, 0x25, 0xff, 0xfd,
		0xff, 0xff, 0x89, 0x41, 0x04, 0x0f, 0x00, 0x9f, 0x80, 0x01, 0x00, 0x00,
		0x89, 0x59, 0x04, 0x48, 0x8b, 0x27, 0x48, 0x89, 0xe5, 0xff, 0xb7, 0x80,
		0x00, 0x00, 0x00, 0xff, 0x77, 0x10, 0x48, 0x8b, 0x87, 0xa0, 0x01, 0x00,
		0x00, 0x48, 0x8b, 0x8f, 0xb0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0x97, 0xb8,
		0x01, 0x00, 0x00, 0x48, 0x8b, 0x9f, 0xa8, 0x01, 0x00, 0x00, 0x48, 0x8b,
		0xaf, 0xd0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xb7, 0xc8, 0x01, 0x00, 0x00,
		0x4c, 0x8b, 0x87, 0xd8, 0x01, 0x00, 0x00, 0x4c, 0x8b, 0x8f, 0xe0, 0x01,
		0x00, 0x00, 0x4c, 0x8b, 0x97, 0xe8, 0x01, 0x00, 0x00, 0x4c, 0x8b, 0x9f,
		0xf0, 0x01, 0x00, 0x00, 0x4c, 0x8b, 0xa7, 0xf8, 0x01, 0x00, 0x00, 0x4c,
		0x8b, 0xaf, 0x00, 0x02, 0x00, 0x00, 0x4c, 0x8b, 0xb7, 0x08, 0x02, 0x00,
		0x00, 0x4c, 0x8b, 0xbf, 0x10, 0x02, 0x00, 0x00, 0x48, 0x8b, 0xbf, 0xc0,
		0x01, 0x00, 0x00, 0x48, 0xcb
	};

	// Read CR3 of host and guest.
	asm_rdcr3(&hcr3);
	gcr3 = guest_riv.cr3;

	// Find pml4 table addresses.
	g_pml4_table_addr = vt_get_pml4_table_addr(gcr3);
	h_pml4_table_addr = vt_get_pml4_table_addr(hcr3);

	DEBUG_PRINT (("[DEVIRT] (%s) h_pml4_table_addr (cr3 paddr):%lx\n", __func__, h_pml4_table_addr));

	// Map physical memory of pml4 table addresses.
	g_page_mem = (long*) vt_mapmem_page_table (g_pml4_table_addr);
	h_page_mem = (long*) vt_mapmem_page_table (h_pml4_table_addr);

	DEBUG_PRINT (("[DEVIRT] (%s) h_page_mem(pml4 vaddr):%lx\n", __func__, h_page_mem));
	DEBUG_PRINT (("[DEVIRT] (%s) g_page_mem(pml4 vaddr):%lx\n", __func__, g_page_mem));
	
	// Find common empty entry.
	i = vt_get_empty_entry_idx (g_page_mem, h_page_mem);
	if (i == -1){
		printf ("[DEVIRT_ERROR] (%s) No empty entry for both guest and host pml4 table.\n",
				__func__);
		return;
	}
	
	// Backup gcr3 and empty entry index
	// for restoring page table in revirt phase.
	gcr3_bak = shared_area_vaddr + SHARED_AREA_VT_OFFSET_MISC_GCR3_BAK/8;
	empty_entry_idx_bak = shared_area_vaddr + SHARED_AREA_VT_OFFSET_MISC_EMPTY_ENTRY_IDX_BAK/8;
	*gcr3_bak = gcr3;
	*empty_entry_idx_bak = i;

	g_pml4_empty_entry = g_page_mem + i;
	h_pml4_empty_entry = h_page_mem + i;
	
	// Set PDPT base address to the empty entry of guest.
	g_pdp_alloc_addr = vt_alloc_table_to_entry (g_pml4_empty_entry, 0x0);

	// Set all the entries in the PDP to be present.
	for (level3_entry_num = 0; level3_entry_num < 512; level3_entry_num++)
		*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + level3_entry_num) \
			= PTE_P_BIT;
	
	// Set PD base address to the offset 0x1000 from PAGE_TABLE_AREA.
	g_pd_alloc_addr = vt_alloc_table_to_entry (	\
			shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8, 0x1000);
	
	// Set all the entries in the PD table to be present.
	for (level2_entry_num = 0; level2_entry_num < 512; level2_entry_num++)
		*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 \
				+ level2_entry_num) = PTE_P_BIT;
	// Set PT base address.
	g_pt_alloc_addr = vt_alloc_table_to_entry (
			shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8, \
			0x2000);

	// Set PDP table base address to the empty entry of host.
	h_pdp_alloc_addr = vt_alloc_table_to_entry (h_pml4_empty_entry, 0x0);

	// For debug.
	DEBUG_PRINT (("[DEVIRT] (%s) ******** Page table settings:\n", __func__));
	DEBUG_PRINT (("[DEVIRT] (%s) g_pdp_alloc_addr:%llx\n", __func__, g_pdp_alloc_addr));
	DEBUG_PRINT (("[DEVIRT] (%s) g_pd_alloc_addr:%llx\n", __func__, g_pd_alloc_addr));
	DEBUG_PRINT (("[DEVIRT] (%s) g_pt_alloc_addr:%llx\n", __func__, g_pt_alloc_addr));
	DEBUG_PRINT (("[DEVIRT] (%s) h_pdp_alloc_addr:%llx\n", __func__, h_pdp_alloc_addr));
	DEBUG_PRINT (("[DEVIRT] (%s) ****************************\n", __func__));

	// Set all the entries in the PT to be present.
	// 0x200 = 0x1000 / 8
	// mem_long + PAGE_TABLE_AREA/8 + 0x400 is virtual address of 0th PTE
	for (level1_entry_num = 0; level1_entry_num < 512; level1_entry_num++) 
		*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 \
				+ level1_entry_num) = PTE_P_BIT;

	// Allocate the first entry of PT. It points to PA of SWITCH CODE.
	*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8) 	\
		= SHARED_AREA_VT_DEVIRT_SW_CODE_AREA | PTE_P_BIT | PTE_RW_BIT;// No G and US bit.

	// Allocate the second entry of PT.
	// It points to PA of guest states (=SHARED_AREA_VT_GUEST_STATE).
	*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 1) \
		= SHARED_AREA_VT_GUEST_STATE | PTE_P_BIT | PTE_RW_BIT; // No G and US bit.

	DEBUG_PRINT (("[DEVIRT] (%s) substitute switch code start address to PTE 0 - %llx with value %llx\n", __func__, 
			(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8),
			*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8)));
	DEBUG_PRINT (("[DEVIRT] (%s) substitute shared area(0x48000000) to PTE 1 - %llx with value:%llx\n", __func__, 
			(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 1),
			*(shared_area_vaddr + SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 1)));

	// 0x1000 is vmcb size
	// i<<39 is VA of 0x49000000 (switch_code will be located in physical address 0x49000000)
	// i<<39 + 0x1000 is virtual address of shared area (0x48000000)

	// copy switch_code to shared_area_vaddr + OFFSET_DEVIRT_SW_CODE_AREA. 
	// [WARNING] void pointer is added by 1 not 8. 
	switch_code_size = sizeof(switch_code) / sizeof(char);
	memcpy ((void*)(shared_area_vaddr + SHARED_AREA_VT_OFFSET_DEVIRT_SW_CODE_AREA/8), \
			(void*)switch_code, switch_code_size);

	/* Preparation for REVIRT. */
	// Modify host page table to map (kernel) virtual address 0xffff884049000000 to 0x49000000
	// FFFF884049000000 mapping start
	vt_alloc_table_to_entry (h_page_mem + 0x110, 0x4000); //0x110=0b100010000 <= 8800's first left 9 bit
	vt_alloc_table_to_entry (shared_area_vaddr \
			+ (SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA + SHARED_AREA_VT_OFFSET_HOST_PDT_ADDR)/8 \
			+ 0x101, \
			0x5000);   
	vt_alloc_table_to_entry (shared_area_vaddr \
			+ (SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA + SHARED_AREA_VT_OFFSET_HOST_PT_ADDR)/8 \
			+ 0x48, \
			0x6000); 
	*(shared_area_vaddr \
			+ (SHARED_AREA_VT_OFFSET_PAGE_TABLE_AREA + SHARED_AREA_VT_OFFSET_HOST_SW_CODE_ADDR)/8 \
			+ 0x0) \
		= SHARED_AREA_VT_DEVIRT_SW_CODE_AREA | PTE_P_BIT | PTE_RW_BIT;	// No US bit.

	DEBUG_PRINT(("[DEVIRT] (%s) BitVisor CR3 0x110 entry(%llx): %llx\n", __func__, 
				(h_page_mem+0x110), *(h_page_mem + 0x110)));
	DEBUG_PRINT(("[DEVIRT] (%s) PD 0x101 entry(%llx): %llx\n", __func__, 
				(shared_area_vaddr + 0x2000000/8 + 0x4000/8 + 0x101), 
				*(shared_area_vaddr + 0x2000000/8 + 0x4000/8 + 0x101)));
	DEBUG_PRINT(("[DEVIRT] (%s) PT 0x48 entry(%llx): %llx\n", __func__, 
				(shared_area_vaddr + 0x2000000/8 + 0x5000/8 + 0x48),
				*(shared_area_vaddr + 0x2000000/8 + 0x5000/8 + 0x48)));
	DEBUG_PRINT(("[DEVIRT] (%s) 0x4a006000's first addr(%llx): %llx\n", __func__, 
				(shared_area_vaddr + 0x2000000/8 + 0x6000/8 + 0x0), 
				*(shared_area_vaddr + 0x2000000/8 + 0x6000/8 + 0x0)));

	// Unmapmem.
	unmapmem (g_page_mem, 0x1000);
	unmapmem (h_page_mem, 0x1000);
	
	// VMXOFF.
	vt_prepare_resume();
	vt_flush_guest_tlb ();
	vt_vmxoff();

	asm_vt_jump_to_devirt_switch_code (i<<39, (u64)shared_area_vaddr);
}


/**
 * Restore guest VMCS state with guest's native state.
 */
static void
vt_restore_guest_state (long *shared_area_vaddr)
{
	long *native_state_vaddr = NULL;	// Address of native state save area.
	short *mem_short = NULL;

	native_state_vaddr = shared_area_vaddr + SHARED_AREA_VT_OFFSET_NATIVE_STATE_AREA/8;
	mem_short = (short *)native_state_vaddr;

	// Load VMCS ptr.
	vt_vmptrld (current->u.vt.vi.vmcs_region_phys);

	// Guest state area in VMCS.
	vt_write_general_reg (GENERAL_REG_RSP, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RSP));
	vt_write_flags (*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RFLAGS));
	vt_write_ip (*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RIP));
	vt_write_control_reg (CONTROL_REG_CR0, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CR0));
	vt_write_control_reg (CONTROL_REG_CR3, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CR3));
	vt_write_control_reg (CONTROL_REG_CR4, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CR4) | CR4_VMXE_BIT);
	asm_vmwrite (VMCS_GUEST_DR7, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DR7));

	vt_write_msr(MSR_IA32_EFER, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_EFER));
	vt_write_msr(MSR_IA32_SYSENTER_CS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_CS));
	vt_write_msr(MSR_IA32_SYSENTER_ESP, \
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_ESP));
	vt_write_msr(MSR_IA32_SYSENTER_EIP,	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SYSENTER_EIP));
	//vt_write_msr(MSR_IA32_DEBUGCTL, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DEBUGCTL));
	//vt_write_msr(MSR_IA32_PAT, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_PAT));
	//vt_write_msr(MSR_IA32_PERF_GLOBAL_CTRL, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_PERF_GLOBAL));
	//vt_write_msr(MSR_IA32_BNDCFGS, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_BNDCFGS));

	// Do not save SMBASE. Assuming it is not changed.
	//smbase
	
	// Segment Registers.
	//asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CS_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_CS_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CS_LIMIT));
	asm_vmwrite (VMCS_GUEST_CS_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CS_SEL));
	//asm_vmwrite (VMCS_GUEST_CS_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CS_BASE));

	//asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DS_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_DS_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DS_LIMIT));
	asm_vmwrite (VMCS_GUEST_DS_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DS_SEL));
	//asm_vmwrite (VMCS_GUEST_DS_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_DS_BASE));

	//asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_ES_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_ES_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_ES_LIMIT));
	asm_vmwrite (VMCS_GUEST_ES_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_ES_SEL));
	//asm_vmwrite (VMCS_GUEST_ES_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_ES_BASE));

	//asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FS_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_FS_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FS_LIMIT));
	asm_vmwrite (VMCS_GUEST_FS_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FS_SEL));
//	asm_vmwrite (VMCS_GUEST_FS_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FS_BASE));
	vt_write_msr(MSR_IA32_FS_BASE, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FS_BASE));

	//asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GS_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_GS_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GS_LIMIT));
	asm_vmwrite (VMCS_GUEST_GS_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GS_SEL));
//	asm_vmwrite (VMCS_GUEST_GS_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GS_BASE));
	vt_write_msr(MSR_IA32_GS_BASE, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GS_BASE));

	//asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SS_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_SS_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SS_LIMIT));
	asm_vmwrite (VMCS_GUEST_SS_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SS_SEL));
	//asm_vmwrite (VMCS_GUEST_SS_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_SS_BASE));

	vt_write_gdtr (*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_GDTR_BASE),
			*(mem_short+ SHARED_AREA_VT_OFFSET_NATIVE_GDTR_LIMIT));

	// TODO ldtr is not used? (sel=0)
	//asm_vmwrite (VMCS_GUEST_LDTR_ACCESS_RIGHTS, 	\
			//*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_LDTR_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_LDTR_LIMIT,			\
			//*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_LDTR_LIMIT));
	//asm_vmwrite (VMCS_GUEST_LDTR_SEL,				\
			//(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_LDTR_SEL));
	//asm_vmwrite (VMCS_GUEST_LDTR_BASE,			\
			//*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_LDTR_BASE));

	vt_write_idtr (*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_IDTR_BASE),
			*(mem_short+ SHARED_AREA_VT_OFFSET_NATIVE_IDTR_LIMIT));

	//asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, 	\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_TR_ACCESS_RIGHT));
	//asm_vmwrite (VMCS_GUEST_TR_LIMIT,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_TR_LIMIT));
	asm_vmwrite (VMCS_GUEST_TR_SEL,				\
			(u16)*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_TR_SEL));
	//asm_vmwrite (VMCS_GUEST_TR_BASE,			\
			*(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_TR_BASE));

	// The other vcpu registers.
	vt_write_general_reg (GENERAL_REG_RAX, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RAX));
	vt_write_general_reg (GENERAL_REG_RBX, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RBX));
	vt_write_general_reg (GENERAL_REG_RCX, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RCX));
	vt_write_general_reg (GENERAL_REG_RDX, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RDX));
	vt_write_general_reg (GENERAL_REG_RDI, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RDI));
	vt_write_general_reg (GENERAL_REG_RSI, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RSI));
	vt_write_general_reg (GENERAL_REG_RBP, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_RBP));
	vt_write_general_reg (GENERAL_REG_R8,  *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R8));
	vt_write_general_reg (GENERAL_REG_R9,  *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R9));
	vt_write_general_reg (GENERAL_REG_R10, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R10));
	vt_write_general_reg (GENERAL_REG_R11, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R11));
	vt_write_general_reg (GENERAL_REG_R12, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R12));
	vt_write_general_reg (GENERAL_REG_R13, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R13));
	vt_write_general_reg (GENERAL_REG_R14, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R14));
	vt_write_general_reg (GENERAL_REG_R15, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_R15));
	vt_write_control_reg (CONTROL_REG_CR2, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_CR2));
	//vt_write_control_reg (CONTROL_REG_CR8, );
	
	vt_write_msr(MSR_IA32_STAR, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_STAR));
	vt_write_msr(MSR_IA32_LSTAR, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_LSTAR));
	vt_write_msr(MSR_IA32_FMASK, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_FMASK));
	vt_write_msr(MSR_IA32_KERNEL_GS_BASE, *(native_state_vaddr+ SHARED_AREA_VT_OFFSET_NATIVE_KERNEL_GSBASE));
}

/**
 * Delete the mapping in the guest-page table that created during devirt process.
 */
void
vt_unmap_guest_pt (long *shared_area_vaddr)
{
	int empty_entry_idx = 0;
	ulong hcr3;
	long *gcr3_bak = NULL;
	long *empty_entry_idx_bak = NULL;
	void *g_pml4_table_addr = NULL;
	void *h_pml4_table_addr = NULL;
	long * g_page_mem = NULL;
	long * h_page_mem = NULL;

	// Back up guest page table and BitVisor page table.
	// It is changed during DEVIRT.
	// Find page-map level-4 table base address in CR3
	asm_rdcr3 (&hcr3);
	
	gcr3_bak = shared_area_vaddr + SHARED_AREA_VT_OFFSET_MISC_GCR3_BAK/8;
	empty_entry_idx_bak = shared_area_vaddr + SHARED_AREA_VT_OFFSET_MISC_EMPTY_ENTRY_IDX_BAK/8;

	// Find pml4 table addresses.
	g_pml4_table_addr = vt_get_pml4_table_addr (*gcr3_bak);
	h_pml4_table_addr = vt_get_pml4_table_addr (hcr3);

	// Map physical memory of pml4 table addresses.
	g_page_mem = (long*) vt_mapmem_page_table (g_pml4_table_addr);
	h_page_mem = (long*) vt_mapmem_page_table (h_pml4_table_addr);

	empty_entry_idx = *empty_entry_idx_bak;

	*(g_page_mem + empty_entry_idx) = 0;
	*(h_page_mem + empty_entry_idx) = 0;

	unmapmem (g_page_mem, 0x1000);
	unmapmem (h_page_mem, 0x1000);
}


// Unmap mappings for host.
static void
vt_unmap_host_pt (void)
{
	// Mappings used for HRIV and MISC area. Used in revirt.
	unmapmem (sa_hriv, 0x500);
	unmapmem (sa_misc, 0x500);
}

void
vt_do_resume (void)
{
	ASSERT (current->u.vt.saved_vmcs);
	vt_vmxon ();
	vt_flush_guest_tlb ();
	memcpy (current->u.vt.vi.vmcs_region_virt, current->u.vt.saved_vmcs,
		PAGESIZE);
	asm_vmclear (&current->u.vt.vi.vmcs_region_phys);
	asm_vmptrld (&current->u.vt.vi.vmcs_region_phys);
	current->u.vt.first = true;

	/* TODO we need to consider locking mechanism 
	 * when it is run on multi-core system.
	 */
	//free_page (current->u.vt.saved_vmcs);
	//current->u.vt.saved_vmcs = NULL;
	//spinlock_init (&currentcpu->suspend_lock);
	//spinlock_lock (&currentcpu->suspend_lock);
}

/**
 * Do post-jobs after revirtualized.
 */
void
vt_revirt_post (long *shared_area_vaddr)
{
	// Unmap the mapping in guest pml4 table.
	vt_unmap_guest_pt(shared_area_vaddr);

	// Unmap the mapping used for HRIV and MISC area.
	vt_unmap_host_pt();

	// Do VMXON, VMCLEAR, and PTRLD.
	vt_do_resume();

	// Restore guest state.
	vt_restore_guest_state (shared_area_vaddr);
}

/**
 * This function is re-executed after REVIRT
 * because the host's state is saved before it is called.
 */
void
vt_re_exe (long *shared_area_vaddr)
{
	// Read host state. It is inline function.
	vt_read_host_state();

	// Re-executed code.
	if (before_devirt) {	/* before_devirt == true. */

		// Save host state into SHARED_AREA. The state has already been read.
		vt_save_host_state (shared_area_vaddr);

		// Save guest area of VMCS into SHARED_AREA.
		vt_save_guest_state (shared_area_vaddr);

		before_devirt = false;

		// Hand over control to Guest OS.
		vt_devirt_handover(shared_area_vaddr);

	} else {	/* before_devirt == false. */

		// Do post-REVIRT jobs.
		vt_revirt_post(shared_area_vaddr);

		// Unset devirt_grant bit.
		devirt_grant = false;
		printf("Revirt Done.\n");
	}
}


/**
 * Devirtualize main function.
 * It is called from vt_mainloop() in vt_main.c.
 * But if _DIRECT_DEVIRT_ is defined, it is called by devirt_vmmcall() in vmmcall_devirt.c
 */
void
vt_devirtualize ()
{
	long *shared_area = NULL;	// for de/revirt.

	// Check devirtualizable (CPL, IF bit).
	if (!vt_devirtualizable()) {
		printf ("[DEVIRT] (%s) It is not devirtualizable.\n", __func__);	
		return;
	}

	DEBUG_PRINT(("[DEVIRT] (%s) Start devirtualization.\n", __func__));
	before_devirt = true;

	// Map SHARED_AREA.
	shared_area = vt_map_shared_area();
	
	// Re-executed code section after REVIRT.
	// Do handover and post-REVIRT jobs in this function.
	vt_re_exe(shared_area);
}
