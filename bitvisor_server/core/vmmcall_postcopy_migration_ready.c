#include "printf.h"
#include "process.h"
#include "vt_regs.h"
#include "vt_msr.h"
#include "regs.h"
#include "constants.h"
#include "current.h"
#include "vmmcall.h"
#include "initfunc.h"
#include "sleep.h"
#include "vt_paging.h"
#include "cache.h"
#include "time.h"

#define _PRINT_DEBUG_POST_MIG_READY_  // uncomment for DEBUG print.

#ifdef _PRINT_DEBUG_POST_MIG_READY_
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x) do {} while (0)
#endif

#define DEBUG_PRINT_VAL(x) DEBUG_PRINT(("[POSTCOPY_MIGRATION_READY] (%s) %s:%lx\n", __func__, #x, x))

static int stop_value;
static unsigned long* vmem_for_cpu;
struct regs_in_vmcs guest_riv;
static ulong grax, grbx, grcx, grdx, grdi, grsi, grbp, grsp,	\
		     gr8, gr9, gr10, gr11, gr12, gr13, gr14, gr15,	\
		     gcr2, grip,				\
		     gefer, gsysenter_cs, gsysenter_esp, gsysenter_eip,	\
		     gfs_base, ggs_base;

/* Print read guest state. (Print static variables) */
	static void  // FIXME erase this function after migration success
print_read_guest_state (void)
{
	DEBUG_PRINT(("[MIG] (%s) ******* Read Guest State *******\n", __func__));
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

	DEBUG_PRINT(("[MIG] (%s) $$$$$$$ Read Guest State END $$$$$$$\n", __func__));
}

/** Print for DEBUG.
 */
static void  // FIXME erase this function after migration success
vt_print_val(const char *func_name, long* mem_vaddr, char *name){
	DEBUG_PRINT(("[MIG] (%s) %s\t%lx\n", func_name, name, *(mem_vaddr)));
}

/* Print saved guest state. (Print values in memory) */
	static void  // FIXME erase this function after migration success
print_saved_guest_state (long *mem_long, short *mem_short)
{
	DEBUG_PRINT(("[MIG] (%s) ******* Saved Guest State *******\n", __func__));

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

	DEBUG_PRINT(("[MIG] (%s) $$$$$$$ Saved Guest State END $$$$$$$\n", __func__));
}

	static void
load_guest_state_to_vmcs ()
{
	unsigned short* vmem_for_cpu_short = (unsigned short*)vmem_for_cpu;

	//print_saved_guest_state(vmem_for_cpu, vmem_for_cpu_short);

	grsp = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RSP);
	guest_riv.rflags = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RFLAGS);
	grip = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RIP);
	guest_riv.cr0 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_CR0);
	guest_riv.cr3 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_CR3);
	guest_riv.cr4 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_CR4);
	guest_riv.dr7 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_DR7);
	gefer = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_EFER);
	gsysenter_cs = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_SYSENTER_CS);
	gsysenter_esp = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_SYSENTER_ESP);
	gsysenter_eip = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_SYSENTER_EIP);

	// Segment Registers
	guest_riv.cs.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_CS_SEL);
	guest_riv.ds.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_DS_SEL);
	guest_riv.es.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_ES_SEL);
	guest_riv.fs.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_FS_SEL);
	gfs_base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_FS_BASE);	// JYKIM added.
	guest_riv.gs.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_GS_SEL);
	ggs_base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_GS_BASE);	// JYKIM added.
	guest_riv.ss.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_SS_SEL);
	guest_riv.gdtr.limit = *(vmem_for_cpu_short+ SHARED_AREA_VT_OFFSET_GDTR_LIMIT);
	guest_riv.gdtr.base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_GDTR_BASE);
	guest_riv.idtr.limit = *(vmem_for_cpu_short+ SHARED_AREA_VT_OFFSET_IDTR_LIMIT);
	guest_riv.idtr.base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_IDTR_BASE);
	guest_riv.tr.sel = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_TR_SEL);

	// The other vcpu registers.
	grax = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RAX);
	grbx = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RBX);
	grcx = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RCX);
	grdx = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RDX);
	grdi = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RDI);
	grsi = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RSI);
	grbp = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_RBP);
	gr8 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R8);
	gr9 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R9);
	gr10 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R10);
	gr11 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R11);
	gr12 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R12);
	gr13 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R13);
	gr14 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R14);
	gr15 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R15);
	gcr2 = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_CR2);

	current->u.vt.msr.star = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_STAR);
	current->u.vt.msr.lstar = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_LSTAR);
	current->u.vt.msr.fmask = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_FMASK);
	current->u.vt.msr.kerngs = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_KERNEL_GSBASE);

	// print read guest state
	//print_read_guest_state();

	// load to vmcs
	// load vmcs prt
	vt_vmptrld(current->u.vt.vi.vmcs_region_phys);

	vt_write_general_reg(GENERAL_REG_RSP, grsp);
	vt_write_flags(guest_riv.rflags);
	vt_write_ip(grip);
	vt_write_control_reg(CONTROL_REG_CR0, guest_riv.cr0);
	vt_write_control_reg(CONTROL_REG_CR3, guest_riv.cr3);
	vt_write_control_reg(CONTROL_REG_CR4, guest_riv.cr4);

	asm_vmwrite(VMCS_GUEST_DR7, guest_riv.dr7);
	vt_write_msr(MSR_IA32_EFER, gefer);
	vt_write_msr(MSR_IA32_SYSENTER_CS, gsysenter_cs);
	vt_write_msr(MSR_IA32_SYSENTER_ESP, gsysenter_esp);
	vt_write_msr(MSR_IA32_SYSENTER_EIP, gsysenter_eip);

	asm_vmwrite(VMCS_GUEST_CS_SEL, guest_riv.cs.sel);
	asm_vmwrite(VMCS_GUEST_DS_SEL, guest_riv.ds.sel);
	asm_vmwrite(VMCS_GUEST_ES_SEL, guest_riv.es.sel);
	asm_vmwrite(VMCS_GUEST_FS_SEL, guest_riv.fs.sel);
	vt_write_msr(MSR_IA32_FS_BASE, gfs_base);
	asm_vmwrite(VMCS_GUEST_GS_SEL, guest_riv.gs.sel);
	vt_write_msr(MSR_IA32_GS_BASE, ggs_base);
	asm_vmwrite(VMCS_GUEST_SS_SEL, guest_riv.ss.sel);

	vt_write_gdtr(guest_riv.gdtr.base, guest_riv.gdtr.limit);
	vt_write_idtr(guest_riv.idtr.base, guest_riv.idtr.limit);
	asm_vmwrite(VMCS_GUEST_TR_SEL, guest_riv.tr.sel);

	// other vcpu registers
	vt_write_general_reg (GENERAL_REG_RAX, grax);
	vt_write_general_reg (GENERAL_REG_RBX, grbx);
	vt_write_general_reg (GENERAL_REG_RCX, grcx);
	vt_write_general_reg (GENERAL_REG_RDX, grdx);
	vt_write_general_reg (GENERAL_REG_RDI, grdi);
	vt_write_general_reg (GENERAL_REG_RSI, grsi);
	vt_write_general_reg (GENERAL_REG_RBP, grbp);
	vt_write_general_reg (GENERAL_REG_R8, gr8);
	vt_write_general_reg (GENERAL_REG_R9, gr9);
	vt_write_general_reg (GENERAL_REG_R10, gr10);
	vt_write_general_reg (GENERAL_REG_R11, gr11);
	vt_write_general_reg (GENERAL_REG_R12, gr12);
	vt_write_general_reg (GENERAL_REG_R13, gr13);
	vt_write_general_reg (GENERAL_REG_R14, gr14);
	vt_write_general_reg (GENERAL_REG_R15, gr15);
	vt_write_control_reg (CONTROL_REG_CR2, gcr2);

	vt_write_msr(MSR_IA32_STAR, current->u.vt.msr.star);
	vt_write_msr(MSR_IA32_LSTAR, current->u.vt.msr.lstar);
	vt_write_msr(MSR_IA32_FMASK, current->u.vt.msr.fmask);
	vt_write_msr(MSR_IA32_KERNEL_GS_BASE, current->u.vt.msr.kerngs);


	// JYKIM added. TODO.
	asm_vmwrite (VMCS_CR4_READ_SHADOW, guest_riv.cr4 & 0xffffffffffffdfff);
	asm_vmwrite(VMCS_GUEST_CS_ACCESS_RIGHTS, 0xa09b);
	asm_vmwrite(VMCS_GUEST_SS_ACCESS_RIGHTS, 0x1c000);
	asm_vmwrite(VMCS_GUEST_FS_BASE, gfs_base);
	asm_vmwrite(VMCS_GUEST_GS_BASE, ggs_base);

	t_sd_base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_T_SD_BASE);
	r_sd_base = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_R_SD_BASE);
	//printf("t_sd_base and r_sd_base received: %x, %x\n", t_sd_base, r_sd_base);
	virtio_send_queue = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_VIRTIO_SEND_QUEUE);
	virtio_recv_queue = *(vmem_for_cpu + SHARED_AREA_VT_OFFSET_VIRTIO_RECV_QUEUE);
	//printf("virtio_send_queue: %x, virtio_recv_queue: %x\n", virtio_send_queue, virtio_recv_queue);
	mig_flag_for_mac = 1;
	//printf("Load guest state to VMCS\n");
}

static int
send_msg_postcopy_ready ()
{
	int ret_echoctl = -1;
	u64 array_echoctl[3];
	struct msgbuf mbuf_echoctl;
	int d_echoctl;

	d_echoctl = msgopen("echoctl");
	if (d_echoctl < 0) {
		printf ("echoctl msg descriptor not found.\n");
		return;
	}

	array_echoctl[0] = 5;	// new msg type
	setmsgbuf (&mbuf_echoctl, array_echoctl, sizeof array_echoctl, 0);
	ret_echoctl = msgsendbuf (d_echoctl, 0, &mbuf_echoctl, 1);
	msgclose (d_echoctl);
	return ret_echoctl;
}

	void
postcopy_migration_ready_vmmcall()
{
	//printf("migration_ready_vmmcall start\n");
	//get_acpi_time (&t_start[0]);
	/*get_acpi_time (&mig_start_t);
	new_start_t = mig_start_t;*/

	// Dump VMCS.
	/*
	printf("Dump VMCS BEFORE migration!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	vmcs_dump_vcpu();
	*/
	stop_value = 1;
	//printf("migration_ready_vmmcall: unregister_virtio_handler\n");
	core_io_unregister_virtio_handler();	// 190318

	flush_cache();

	send_msg_postcopy_ready();

	while (stop_value) {
		schedule();
	}

	//printf("migration_ready_vmmcall: I received cpu states\n");
	//after recv cpu state

	//printf("migration_ready_vmmcall: register virtio_handler\n");
	core_io_register_virtio_handler();	// 190318

	// Load source's CPU states to VMCB and other BitVisor's data structures
	load_guest_state_to_vmcs();

	//printf("postcopy_ready_vmmcall: vt_paging_clear, tlbflush, flush guest tlb\n");
	vt_paging_clear_all();
	vt_paging_tlbflush();
	vt_paging_flush_guest_tlb();

	// Dump VMCS.
	//printf("Dump VMCS AFTER migration!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	//vmcs_dump_vcpu();

	current->u.vt.aft_mig = 1;
	aft_mig_in_mmio = 1;
	//desc_flag = true;
	//ept_recv_flag = true;

	time_measure = true;
	//get_acpi_time (&mig_start_t);
	mig_start_t = get_cpu_time();
	new_start_t = mig_start_t;

	//printf("postcopy_ready_vmmcall: postcopy vmmcall exit\n");

	//panic();
}

	static void
vmmcall_postcopy_migration_ready_init (void)
{
	vmmcall_register("postcopy_migration_ready", postcopy_migration_ready_vmmcall);
}

	static int
postcopy_migration_ready_sub (unsigned long (*array)[2], int len)
{
	int ret;
	ulong cmd;
	struct arg *a;

	if(len != sizeof *array)
		return -1;
	cmd = (*array)[0];
	vmem_for_cpu = (*array)[1];

	switch(cmd) {
		case 0:
			ret = -1;
			break;
		case 1:
			stop_value = 0;
			ret = 0;
			//printf("stop_value is changed to zero\n");
			break;
		default:
			ret = -1;
	}

	//printf("postcopy_migration_ready_sub called\n");

	return ret;
}

	static int
postcopy_migration_ready_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if(m != MSG_BUF)
		return -1;
	if(bufcnt != 1)
		return -1;
	return postcopy_migration_ready_sub(buf[0].base, buf[0].len);
}

	static void
postcopy_migration_ready_init_msg (void)
{
	msgregister("postcopy_ready", postcopy_migration_ready_msghandler);
}

INITFUNC("msg0", postcopy_migration_ready_init_msg);
INITFUNC("vmmcal0", vmmcall_postcopy_migration_ready_init);
