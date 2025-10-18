/* Author Jongyul Kim.
 * Added for VMCS dump.
 * The code is from xen.
 */
#include "constants.h"
#include "vt.h"
#include "initfunc.h"
#include "printf.h"
#include "vt_vmcs.h"
#include "current.h"
#include "vt_regs.h"
#include <xen/vmcs.h>
#include <xen/xen-x86_64.h>

#define X86_CR4_PAE        0x00000020 /* enable physical address extensions */
#define _EFER_LMA		10 /* Long mode active (read-only) */
#define EFER_LMA		(1<<_EFER_LMA)

#define vmr16(fld) ({             \
    (u16)vmr(fld);           \
})

#define vmr32(fld) ({                         \
    (u32)vmr(fld);                       \
})

# if BITS_PER_LONG == 64
#  define __PRI64_PREFIX	"l"
# else
#  define __PRI64_PREFIX	"ll"
# endif

# define PRIx64		__PRI64_PREFIX "x"

static inline ulong vmr(ulong addr)
{
	ulong val;
	asm_vmread(addr, &val); 
	return val;
}

static void vmx_dump_sel(char *name, u32 selector)
{
    u32 sel, attr, limit;
    ulong base;
    sel = vmr(selector);
    attr = vmr(selector + (GUEST_ES_AR_BYTES - GUEST_ES_SELECTOR));
    limit = vmr(selector + (GUEST_ES_LIMIT - GUEST_ES_SELECTOR));
    base = vmr(selector + (GUEST_ES_BASE - GUEST_ES_SELECTOR));
    printf("%s: %04x %05x %08x %016"PRIx64"\n", name, sel, attr, limit, base);
}

static void vmx_dump_sel2(char *name, u32 lim)
{
    u32 limit;
    ulong base;
    limit = vmr(lim);
    base = vmr(lim + (GUEST_GDTR_BASE - GUEST_GDTR_LIMIT));
    printf("%s:            %08x %016"PRIx64"\n", name, limit, base);
}


//void vmcs_dump_vcpu(struct vcpu *v)
void vmcs_dump_vcpu(void)
{
    u32 vmentry_ctl, vmexit_ctl;
    ulong cr4;
    ulong efer;
	ulong grsp, grip;
	ulong exec_control;
	ulong secondary_exec_control;
	ulong pin_based_exec_control;
    unsigned int i, n;
	struct regs_in_vmcs guest_riv;

	// Lock starts if multi threaded.

	// Load VMCS ptr
	vt_vmptrld (current->u.vt.vi.vmcs_region_phys);

	// read guest area of VMCS
	vt_get_vmcs_regs_in_vmcs (&guest_riv);
	vt_read_general_reg (GENERAL_REG_RSP, &grsp);
	vt_read_ip (&grip);

	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &secondary_exec_control);
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &exec_control);
	asm_vmread (VMCS_PIN_BASED_VMEXEC_CTL, &pin_based_exec_control);

    vmentry_ctl = vmr32(VM_ENTRY_CONTROLS),
    vmexit_ctl = vmr32(VM_EXIT_CONTROLS);
    cr4 = vmr(GUEST_CR4);
    efer = vmr(GUEST_EFER);

    printf("*** Guest State ***\n");
    printf("CR0: actual=0x%016lx, shadow=0x%016lx, gh_mask=%016lx\n",
           vmr(GUEST_CR0), vmr(CR0_READ_SHADOW), vmr(CR0_GUEST_HOST_MASK));
    printf("CR4: actual=0x%016lx, shadow=0x%016lx, gh_mask=%016lx\n",
           cr4, vmr(CR4_READ_SHADOW), vmr(CR4_GUEST_HOST_MASK));
    printf("CR3 = 0x%016lx\n", vmr(GUEST_CR3));
    if ( (secondary_exec_control &
          SECONDARY_EXEC_ENABLE_EPT) &&
         (cr4 & X86_CR4_PAE) && !(efer & EFER_LMA) )
    {
        printf("PDPTE0 = 0x%016lx  PDPTE1 = 0x%016lx\n",
               vmr(GUEST_PDPTE(0)), vmr(GUEST_PDPTE(1)));
        printf("PDPTE2 = 0x%016lx  PDPTE3 = 0x%016lx\n",
               vmr(GUEST_PDPTE(2)), vmr(GUEST_PDPTE(3)));
    }
    printf("RSP = 0x%016lx (0x%016lx)  RIP = 0x%016lx (0x%016lx)\n",
           vmr(GUEST_RSP), grsp,
           vmr(GUEST_RIP), grip);
    printf("RFLAGS=0x%08lx (0x%08lx)  DR7 = 0x%016lx (0x%016lx)\n",
           vmr(GUEST_RFLAGS), guest_riv.rflags,
           vmr(GUEST_DR7), guest_riv.dr7);
    printf("Sysenter RSP=%016lx CS:RIP=%04x:%016lx\n",
           vmr(GUEST_SYSENTER_ESP),
           vmr32(GUEST_SYSENTER_CS), vmr(GUEST_SYSENTER_EIP));
    printf("       sel  attr  limit   base\n");
    vmx_dump_sel("  CS", GUEST_CS_SELECTOR);
    vmx_dump_sel("  DS", GUEST_DS_SELECTOR);
    vmx_dump_sel("  SS", GUEST_SS_SELECTOR);
    vmx_dump_sel("  ES", GUEST_ES_SELECTOR);
    vmx_dump_sel("  FS", GUEST_FS_SELECTOR);
    vmx_dump_sel("  GS", GUEST_GS_SELECTOR);
    vmx_dump_sel2("GDTR", GUEST_GDTR_LIMIT);
    vmx_dump_sel("LDTR", GUEST_LDTR_SELECTOR);
    vmx_dump_sel2("IDTR", GUEST_IDTR_LIMIT);
    vmx_dump_sel("  TR", GUEST_TR_SELECTOR);
    if ( (vmexit_ctl & (VM_EXIT_SAVE_GUEST_PAT | VM_EXIT_SAVE_GUEST_EFER)) ||
         (vmentry_ctl & (VM_ENTRY_LOAD_GUEST_PAT | VM_ENTRY_LOAD_GUEST_EFER)) )
        printf("EFER = 0x%016lx  PAT = 0x%016lx\n", efer, vmr(GUEST_PAT));
    printf("PreemptionTimer = 0x%08x  SM Base = 0x%08x\n",
           vmr32(GUEST_PREEMPTION_TIMER), vmr32(GUEST_SMBASE));
    printf("DebugCtl = 0x%016lx  DebugExceptions = 0x%016lx\n",
           vmr(GUEST_IA32_DEBUGCTL), vmr(GUEST_PENDING_DBG_EXCEPTIONS));
    if ( vmentry_ctl & (VM_ENTRY_LOAD_PERF_GLOBAL_CTRL | VM_ENTRY_LOAD_BNDCFGS) )
        printf("PerfGlobCtl = 0x%016lx  BndCfgS = 0x%016lx\n",
               vmr(GUEST_PERF_GLOBAL_CTRL), vmr(GUEST_BNDCFGS));
    printf("Interruptibility = %08x  ActivityState = %08x\n",
           vmr32(GUEST_INTERRUPTIBILITY_INFO), vmr32(GUEST_ACTIVITY_STATE));
    if ( secondary_exec_control &
         SECONDARY_EXEC_VIRTUAL_INTR_DELIVERY )
        printf("InterruptStatus = %04x\n", vmr16(GUEST_INTR_STATUS));

    printf("*** Host State ***\n");
    printf("RIP = 0x%016lx (%ps)  RSP = 0x%016lx\n",
           vmr(HOST_RIP), (void *)vmr(HOST_RIP), vmr(HOST_RSP));
    printf("CS=%04x SS=%04x DS=%04x ES=%04x FS=%04x GS=%04x TR=%04x\n",
           vmr16(HOST_CS_SELECTOR), vmr16(HOST_SS_SELECTOR),
           vmr16(HOST_DS_SELECTOR), vmr16(HOST_ES_SELECTOR),
           vmr16(HOST_FS_SELECTOR), vmr16(HOST_GS_SELECTOR),
           vmr16(HOST_TR_SELECTOR));
    printf("FSBase=%016lx GSBase=%016lx TRBase=%016lx\n",
           vmr(HOST_FS_BASE), vmr(HOST_GS_BASE), vmr(HOST_TR_BASE));
    printf("GDTBase=%016lx IDTBase=%016lx\n",
           vmr(HOST_GDTR_BASE), vmr(HOST_IDTR_BASE));
    printf("CR0=%016lx CR3=%016lx CR4=%016lx\n",
           vmr(HOST_CR0), vmr(HOST_CR3), vmr(HOST_CR4));
    printf("Sysenter RSP=%016lx CS:RIP=%04x:%016lx\n",
           vmr(HOST_SYSENTER_ESP),
           vmr32(HOST_SYSENTER_CS), vmr(HOST_SYSENTER_EIP));
    if ( vmexit_ctl & (VM_EXIT_LOAD_HOST_PAT | VM_EXIT_LOAD_HOST_EFER) )
        printf("EFER = 0x%016lx  PAT = 0x%016lx\n", vmr(HOST_EFER), vmr(HOST_PAT));
    if ( vmexit_ctl & VM_EXIT_LOAD_PERF_GLOBAL_CTRL )
        printf("PerfGlobCtl = 0x%016lx\n",
               vmr(HOST_PERF_GLOBAL_CTRL));

    printf("*** Control State ***\n");
    printf("PinBased=%08x CPUBased=%08x SecondaryExec=%08x\n",
           vmr32(PIN_BASED_VM_EXEC_CONTROL),
           vmr32(CPU_BASED_VM_EXEC_CONTROL),
           vmr32(SECONDARY_VM_EXEC_CONTROL));
    printf("EntryControls=%08x ExitControls=%08x\n", vmentry_ctl, vmexit_ctl);
    printf("ExceptionBitmap=%08x PFECmask=%08x PFECmatch=%08x\n",
           vmr32(EXCEPTION_BITMAP),
           vmr32(PAGE_FAULT_ERROR_CODE_MASK),
           vmr32(PAGE_FAULT_ERROR_CODE_MATCH));
    printf("VMEntry: intr_info=%08x errcode=%08x ilen=%08x\n",
           vmr32(VM_ENTRY_INTR_INFO),
           vmr32(VM_ENTRY_EXCEPTION_ERROR_CODE),
           vmr32(VM_ENTRY_INSTRUCTION_LEN));
    printf("VMExit: intr_info=%08x errcode=%08x ilen=%08x\n",
           vmr32(VM_EXIT_INTR_INFO),
           vmr32(VM_EXIT_INTR_ERROR_CODE),
           vmr32(VM_EXIT_INSTRUCTION_LEN));
    printf("        reason=%08x qualification=%016lx\n",
           vmr32(VM_EXIT_REASON), vmr(EXIT_QUALIFICATION));
    printf("IDTVectoring: info=%08x errcode=%08x\n",
           vmr32(IDT_VECTORING_INFO), vmr32(IDT_VECTORING_ERROR_CODE));
    printf("TSC Offset = 0x%016lx  TSC Multiplier = 0x%016lx\n",
           vmr(TSC_OFFSET), vmr(TSC_MULTIPLIER));
    if ( (exec_control & CPU_BASED_TPR_SHADOW) ||
         (pin_based_exec_control & PIN_BASED_POSTED_INTERRUPT) )
        printf("TPR Threshold = 0x%02x  PostedIntrVec = 0x%02x\n",
               vmr32(TPR_THRESHOLD), vmr16(POSTED_INTR_NOTIFICATION_VECTOR));
    if ( (secondary_exec_control &
          SECONDARY_EXEC_ENABLE_EPT) )
        printf("EPT pointer = 0x%016lx  EPTP index = 0x%04x\n",
               vmr(EPT_POINTER), vmr16(EPTP_INDEX));
    n = vmr32(CR3_TARGET_COUNT);
    for ( i = 0; i + 1 < n; i += 2 )
        printf("CR3 target%u=%016lx target%u=%016lx\n",
               i, vmr(CR3_TARGET_VALUE(i)),
               i + 1, vmr(CR3_TARGET_VALUE(i + 1)));
    if ( i < n )
        printf("CR3 target%u=%016lx\n", i, vmr(CR3_TARGET_VALUE(i)));
    if ( secondary_exec_control &
         SECONDARY_EXEC_PAUSE_LOOP_EXITING )
        printf("PLE Gap=%08x Window=%08x\n",
               vmr32(PLE_GAP), vmr32(PLE_WINDOW));
    if ( secondary_exec_control &
         (SECONDARY_EXEC_ENABLE_VPID | SECONDARY_EXEC_ENABLE_VM_FUNCTIONS) )
        printf("Virtual processor ID = 0x%04x VMfunc controls = %016lx\n",
               vmr16(VIRTUAL_PROCESSOR_ID), vmr(VM_FUNCTION_CONTROL));

	// Unlock. If multi threaded.
}

/* Dump a section of VMCS */
static void print_section(char *header, ulong start, 
		ulong end, int incr)
{
	ulong addr, j;
	ulong val;
	int code;
	char *fmt[4] = {"0x%04lx ", "0x%016lx ", "0x%08lx ", "0x%016lx "};
	//char *err[4] = {"------ ", "------------------ ", 
		//"---------- ", "------------------ "};

	/* Find width of the field (encoded in bits 14:13 of address) */
	code = (start>>13)&3;

	if (header)
		printf("\t %s", header);

	for (addr=start, j=0; addr<=end; addr+=incr, j++) {

		if (!(j&3))
			printf("\n\t\t0x%08x: ", addr);

		asm_vmread(addr, &val);
		printf(fmt[code], val);
		//else
			//printf("%s", err[code]);
	}

	printf("\n");
}
/* Dump current VMCS */
void vmcs_dump_vcpu_naive(void)
{
	print_section("16-bit Control Fields", 0x0, 0x4, 2);
	print_section("16-bit Guest-State Fields", 0x800, 0x80e, 2);
	print_section("16-bit Host-State Fields", 0xc00, 0xc0c, 2);
	print_section("64-bit Control Fields", 0x2000, 0x2013, 1);
	print_section("64-bit Guest-State Fields", 0x2800, 0x2803, 1);
	print_section("32-bit Control Fields", 0x4000, 0x401c, 2);
	print_section("32-bit RO Data Fields", 0x4400, 0x440e, 2);
	print_section("32-bit Guest-State Fields", 0x4800, 0x482a, 2);
	print_section("32-bit Host-State Fields", 0x4c00, 0x4c00, 2);
	print_section("Natural 64-bit Control Fields", 0x6000, 0x600e, 2);
	print_section("64-bit RO Data Fields", 0x6400, 0x640A, 2);
	print_section("Natural 64-bit Guest-State Fields", 0x6800, 0x6826, 2);
	print_section("Natural 64-bit Host-State Fields", 0x6c00, 0x6c16, 2);
}

/* Print VMCS of all the vCPUs. */
//static void vmcs_dump(unsigned char ch)
//{
//	struct domain *d;
//	struct vcpu *v;
//
//	printf("*********** VMCS Areas **************\n");
//	for_each_domain(d) {
//		printf("\n>>> Domain %d <<<\n", d->domain_id);
//		for_each_vcpu(d, v) {
//
//			/* 
//			 * Presumably, if intel VT is unavailable,
//			 * the very first CPU will not pass this test
//			 */
//			if (!vt_available ()) {
//				printf("\t\tVT is unavailable\n");
//				break;
//			}
//			printf("\tVCPU %d\n", v->vcpu_id);
//
//			if (v != current) {
//				vcpu_pause(v);
//				__vmptrld(virt_to_maddr(v->arch.hvm_vmx.vmcs));
//			}
//
//			vmcs_dump_vcpu_naive();
//
//			if (v != current) {
//				__vmptrld(virt_to_maddr(current->arch.hvm_vmx.vmcs));
//				vcpu_unpause(v);
//			}
//		}
//	}
//
//	printf("**************************************\n");
//}

static int
print_vmcs_msghandler (int m, int c)
{
	// Print VMCS here.
	//vmcs_dump_vcpu_naive();
	vmcs_dump_vcpu();
	return 0;
}

static void
print_vmcs_init_msg (void)
{
	msgregister ("print_vmcs", print_vmcs_msghandler);
}

INITFUNC ("msg0", print_vmcs_init_msg);
