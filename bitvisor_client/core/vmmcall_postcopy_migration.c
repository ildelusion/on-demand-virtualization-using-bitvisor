#include "vmmcall_postcopy_migration.h"
#include "printf.h"
#include "process.h"
#include "vt_regs.h"	//vt_read_general_reg, vt_read_ip, vt_read_control_reg
#include "vt_msr.h"	//vt_read_msr
#include "regs.h"	//GENERAL_REG_RAX...
#include "constants.h"	//MSR_IA32_SYSENTER...
#include "current.h"
#include "mm.h"
#include "assert.h"
#include "thread.h"
#include "vt_vmcs.h"
#include "time.h"

#define CPU_STATE_MEMORY_ADDR_POSTCOPY 0x49005000

//#define _PRINT_DEBUG_POSTCOPY_MIG_	// uncomment for DEBUG print.

#ifdef _PRINT_DEBUG_POSTCOPY_MIG_
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x) do {} while (0)
#endif

#define DEBUG_PRINT_VAL(x) DEBUG_PRINT(("[POSTCOPY_MIGRATION_VMMCALL] (%s) %s:%lx\n", __func__, #x, x))

#define NUM_OF_MEM_AREA 6
#define SIZE_OF_SENDBUF 1024	// Max value: MTU - 40 = 1500-40

struct regs_in_vmcs guest_riv;
struct usable_mem {
	long start_addr;
	long end_addr;
};

/*static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	//{0x100000000, 0x2767fffff},
};*/

//static long num_of_send[NUM_OF_MEM_AREA];

/** Print for DEBUG.
 */
static void	// FIXME erase this function after migration success
vt_print_val(const char *func_name, long* mem_vaddr, char *name){
	DEBUG_PRINT(("[POST_MIG] (%s) %s\t%lx\n", func_name, name, *(mem_vaddr)));
}

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
save_guest_cpu_state (unsigned long* cpu_state_vaddr)
{
	static ulong grax, grbx, grcx, grdx, grdi, grsi, grbp, grsp,	\
		gr8, gr9, gr10, gr11, gr12, gr13, gr14, gr15,	\
		gcr2, grip,				\
		gefer, gsysenter_cs, gsysenter_esp, gsysenter_eip,	\
		gfs_base, ggs_base;
	unsigned long* mem = NULL;
	unsigned short* mem_short = NULL;

	mem = cpu_state_vaddr;
	mem_short = (unsigned short*)mem;

#ifdef _PRINT_DEBUG_POSTCOPY_MIG_
	printf("original vmcs region phys: %lx\n", current->u.vt.vi.vmcs_region_phys);
#endif

	vt_vmptrld(current->u.vt.vi.vmcs_region_phys);

#ifdef _PRINT_DEBUG_POSTCOPY_MIG_
	printf("after vt_vmptrld, vmcs region phys: %lx\n", current->u.vt.vi.vmcs_region_phys);
#endif

	vt_get_vmcs_regs_in_vmcs(&guest_riv);

	// Read other state.
	vt_read_general_reg (GENERAL_REG_RSP, &grsp);
	vt_read_ip (&grip);
	vt_read_msr (MSR_IA32_EFER, &gefer);
	vt_read_msr (MSR_IA32_SYSENTER_CS, &gsysenter_cs);
	vt_read_msr (MSR_IA32_SYSENTER_ESP, &gsysenter_esp);
	vt_read_msr (MSR_IA32_SYSENTER_EIP, &gsysenter_eip);
	vt_read_msr (MSR_IA32_FS_BASE, &gfs_base);
	vt_read_msr (MSR_IA32_GS_BASE, &ggs_base);

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

	// Print read guest state.
	//print_read_guest_state ();

	/** MODIFY GUEST STATE **/
	gefer = 0xd01;	// EFER hard coding.

	/** SAVE GUEST STATE **/
	// Guest state area in VMCS.
	*(mem + SHARED_AREA_VT_OFFSET_RSP		) = grsp;
	*(mem + SHARED_AREA_VT_OFFSET_RFLAGS		) = guest_riv.rflags;
	*(mem + SHARED_AREA_VT_OFFSET_RIP		) = grip;
	*(mem + SHARED_AREA_VT_OFFSET_CR0		) = guest_riv.cr0;
	*(mem + SHARED_AREA_VT_OFFSET_CR3		) = guest_riv.cr3;
	*(mem + SHARED_AREA_VT_OFFSET_CR4		) = guest_riv.cr4;
	*(mem + SHARED_AREA_VT_OFFSET_DR7		) = guest_riv.dr7;
	*(mem + SHARED_AREA_VT_OFFSET_EFER		) = gefer;
	*(mem + SHARED_AREA_VT_OFFSET_SYSENTER_CS	) = gsysenter_cs;
	*(mem + SHARED_AREA_VT_OFFSET_SYSENTER_ESP	) = gsysenter_esp;
	*(mem + SHARED_AREA_VT_OFFSET_SYSENTER_EIP	) = gsysenter_eip;
	*(mem + SHARED_AREA_VT_OFFSET_FS_BASE		) = gfs_base;
	*(mem + SHARED_AREA_VT_OFFSET_GS_BASE		) = ggs_base;
	// Do not save SMBASE. Assuming it is not changed.
	//smbase
	// Segment Registers
	*(mem + SHARED_AREA_VT_OFFSET_CS_SEL		) = guest_riv.cs.sel;
	*(mem + SHARED_AREA_VT_OFFSET_DS_SEL		) = guest_riv.ds.sel;
	*(mem + SHARED_AREA_VT_OFFSET_ES_SEL		) = guest_riv.es.sel;
	*(mem + SHARED_AREA_VT_OFFSET_FS_SEL		) = guest_riv.fs.sel;
	*(mem + SHARED_AREA_VT_OFFSET_GS_SEL		) = guest_riv.gs.sel;
	*(mem + SHARED_AREA_VT_OFFSET_SS_SEL		) = guest_riv.ss.sel;
	*(mem_short+ SHARED_AREA_VT_OFFSET_GDTR_LIMIT	) = guest_riv.gdtr.limit;
	*(mem + SHARED_AREA_VT_OFFSET_GDTR_BASE		) = guest_riv.gdtr.base;	
	*(mem_short+ SHARED_AREA_VT_OFFSET_IDTR_LIMIT	) = guest_riv.idtr.limit;
	*(mem + SHARED_AREA_VT_OFFSET_IDTR_BASE		) = guest_riv.idtr.base;
	*(mem + SHARED_AREA_VT_OFFSET_TR_SEL		) = guest_riv.tr.sel;

	// The other vcpu registers.
	*(mem + SHARED_AREA_VT_OFFSET_RAX		) = grax;
	*(mem + SHARED_AREA_VT_OFFSET_RBX		) = grbx;
	*(mem + SHARED_AREA_VT_OFFSET_RCX		) = grcx;
	*(mem + SHARED_AREA_VT_OFFSET_RDX		) = grdx;
	*(mem + SHARED_AREA_VT_OFFSET_RDI		) = grdi;
	*(mem + SHARED_AREA_VT_OFFSET_RSI		) = grsi;
	*(mem + SHARED_AREA_VT_OFFSET_RBP		) = grbp;
	*(mem + SHARED_AREA_VT_OFFSET_R8		) = gr8;
	*(mem + SHARED_AREA_VT_OFFSET_R9		) = gr9;
	*(mem + SHARED_AREA_VT_OFFSET_R10		) = gr10;
	*(mem + SHARED_AREA_VT_OFFSET_R11		) = gr11;
	*(mem + SHARED_AREA_VT_OFFSET_R12		) = gr12;
	*(mem + SHARED_AREA_VT_OFFSET_R13		) = gr13;
	*(mem + SHARED_AREA_VT_OFFSET_R14		) = gr14;
	*(mem + SHARED_AREA_VT_OFFSET_R15		) = gr15;
	*(mem + SHARED_AREA_VT_OFFSET_CR2		) = gcr2;

	*(mem + SHARED_AREA_VT_OFFSET_STAR		) = current->u.vt.msr.star;
	*(mem + SHARED_AREA_VT_OFFSET_LSTAR		) = current->u.vt.msr.lstar;
	*(mem + SHARED_AREA_VT_OFFSET_FMASK		) = current->u.vt.msr.fmask;
	*(mem + SHARED_AREA_VT_OFFSET_KERNEL_GSBASE	) = current->u.vt.msr.kerngs;

	*(mem + SHARED_AREA_VT_OFFSET_T_SD_BASE		) = t_sd_base;
	*(mem + SHARED_AREA_VT_OFFSET_R_SD_BASE		) = r_sd_base;
	//printf("t_sd_base and r_sd_base send: %x, %x\n", t_sd_base, r_sd_base);

	*(mem + SHARED_AREA_VT_OFFSET_VIRTIO_SEND_QUEUE	) = virtio_send_queue;
	*(mem + SHARED_AREA_VT_OFFSET_VIRTIO_RECV_QUEUE	) = virtio_recv_queue;
	printf("send_queue and receive_queue: %x, %x\n", virtio_send_queue, virtio_recv_queue);
	//*(mem + SHARED_AREA_VT_OFFSET_MAC_ADDR		) = 0x0007e90f4b3d;

	// Print saved guest state.
	//print_saved_guest_state (mem, mem_short);
}

	static unsigned long*
map_guest_cpu_state_area(void)
{
	unsigned long *mem = NULL;
	mem = (unsigned long*)mapmem(MAPMEM_HPHYS | MAPMEM_WRITE, CPU_STATE_MEMORY_ADDR_POSTCOPY, 0x1000);
	ASSERT(mem);
	return mem;
}

// Send message to echoctl.c for requesting state transmission.
static void
send_msg_to_echoctl (unsigned long* mem, long total_num_of_send)
{
	int ret_echoctl = -1;
	unsigned long array_echoctl[3];
	struct msgbuf mbuf_echoctl;
	int d_echoctl;		//echoctl msg descriptor id.

	// Get echoctl msg descriptor.
	d_echoctl = msgopen("echoctl");
	if(d_echoctl < 0) {
		printf("echoctl msg descriptor not found.\n");
		ret_echoctl = -1;
	}

	// Set echoctl msg buffer.
	array_echoctl[0] = 3;	// client send
	array_echoctl[1] = mem;	// JSIM fixed ip/echoctl.c and ip/echo-client.c
	array_echoctl[2] = 0;

	//printf ("(%s) mem:%ld, total_num_of_send:%ld\n", __func__, mem, total_num_of_send);

	setmsgbuf (&mbuf_echoctl, array_echoctl, sizeof array_echoctl, 0);

	ret_echoctl = msgsendbuf(d_echoctl, 0, &mbuf_echoctl, 1);
	msgclose(d_echoctl);
	return ret_echoctl;
}

void
postcopy_migration_vmmcall ()
{
	int ret = -1;
	unsigned long* mem = NULL;
	long i = 0;
	long total_num_of_send = 0;
	u64 time1 = 0, time2 = 0;

#ifdef _PRINT_DEBUG_POSTCOPY_MIG_
	DEBUG_PRINT(("[POSTCOPY_MIG] (%s) is called. Dump VMCS before migration\n", __func__));
	vmcs_dump_vcpu();
#endif
	get_acpi_time(&time1);
	DEBUG_PRINT(("[POSTCOPY_MIG] (%s) unregister virtio_handler\n", __func__));
	core_io_unregister_virtio_handler();
	
	mem = map_guest_cpu_state_area();
	DEBUG_PRINT(("[POSTCOPY_MIG] (%s) mapped memory: %lx\n", __func__, mem));
	save_guest_cpu_state(mem);

	while (dont_start_postcopy) {
		schedule();
	}

	send_msg_to_echoctl (mem, total_num_of_send);
	get_acpi_time(&time2);
	DEBUG_PRINT(("[POSTCOPY_MIG] (%s) migration ready time: %llu\n", __func__, time2 - time1));

	while(1) {
		schedule();
	}

	core_io_register_virtio_handler();
	//vmcs_dump_vcpu();
}


void vmmcall_postcopy_migration_init (void)
{
	vmmcall_register ("postcopy_migration", postcopy_migration_vmmcall);
}

INITFUNC ("vmmcal0", vmmcall_postcopy_migration_init);
