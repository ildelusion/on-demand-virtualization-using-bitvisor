#include <serial.h>
#include <smm.h>
#include <lib.h>
#include <xmalloc.h>
#include <smiutil.h>
#include <timer.h>
#include <mm.h>
#include <define.h>
#include <smm_types.h>

#define NO_CR3_RESTORE_TEST 1
//#define _SERIAL_TEST_

#define REVIRT_SWITCH_CODE 0x49000400
#define STATE_SAVE_AREA_IN_SHARED_AREA 0x49000800
#define STATE_SAVE_AREA_SIZE 0x200
#define PTE_P_BIT           0x1
#define PTE_RW_BIT          0x2
#define PTE_US_BIT          0x4
#define PDPE_PS_BIT			0x80
#define PDPE_G_BIT			0x100
#define	PDPE_ENTRY_NUM		0x101
#define KERNEL_LEVEL4_PGT	0x1c12000	// System.map 's init_level4_pgt check
#define PDT_BASE			0x4a004000	// PDT = level2 PAGE TABLE
#define PT_BASE				0x4a005000	// PT level1 PAGE TABLE
#define GUEST_VMCB_PA_LOCATION		0x49000a10	// Location of guest VMCB physical address
//#define HOST_VMCB_PA_LOCATION	0x49000a10	// Location of hypervisor VMCB physical address	// Maybe, I'll not use this
#define HOST_VMCB_PA	0x49001000	// HOST == Hypervisor
#define HOST_GPRS_STATE	0x49000b00		// It's the part of hypervisor VMCB
#define HOST_GPRS		0x49000a18
#define VMCB_START_STATE_SAVE_AREA	0x80	//0x400 = 8 * 0x80

#define HOST_R15	0x0
#define HOST_R14	0x1
#define HOST_R13	0x2
#define HOST_R12	0x3
#define HOST_R11	0x4
#define HOST_R10	0x5
#define HOST_R9		0x6
#define HOST_R8		0x7
#define HOST_RDI	0x8
#define HOST_RSI	0x9
#define HOST_RBP	0xa
#define HOST_RSP	0xb
#define HOST_RBX	0xc
#define HOST_RDX	0xd
#define HOST_RCX	0xe
#define HOST_AFTER_RCX	0xf

#define SEG_DESC_TABLE_SIZE	0xa0
#define SEG_SIZE	0x60

#define STAR_MSR			0xc0000081
#define LSTAR_MSR			0xc0000082
#define CSTAR_MSR			0xc0000083
#define SFMASK_MSR			0xc0000084
#define KERNELGSBASE_MSR	0xc0000102
#define SYSENTER_CS_MSR		0x0174
#define SYSENTER_ESP_MSR	0x0175
#define SYSENTER_EIP_MSR	0x0176

#define STAR				0x40
#define LSTAR				0x41
#define CSTAR				0x42
#define SFMASK				0x43
#define KERNELGSBASE		0x44
#define SYSENTER_CS			0x45
#define SYSENTER_ESP		0x46
#define SYSENTER_EIP		0x47

#define ES		0x0
#define CS		0x2
#define SS		0x4
#define DS		0x6
#define FS		0x8
#define GS		0xa
#define BITVISOR_STATE_GDTR_LIMIT	0x6f		// 0xde / 2 = 0x6f	// It should be used by short*	// use 0xde when lgdt
#define GDTR_SEL_ATTR	0xc
#define GDTR_BASE	0xd
#define LDTR	0xe
#define BITVISOR_STATE_IDTR_LIMIT	0x7f		// 0xfe / 2 = 0x7f	// It should be used by short*	// use 0xfe when lidt
#define IDTR_SEL_ATTR	0x10
#define IDTR_BASE	0x11
#define TR		0x12
#define EFER	0x1a	//0xd0
#define CR4		0x29	//0x148
#define CR3		0x2a	//0x150
#define CR0		0x2b	//0x158
#define DR7		0x2c	//0x160
#define DR6		0x2d	//0x168
#define RFLAGS	0x2e	//0x170
#define RIP		0x2f	//0x178
#define R15		0x30	//0x180
#define R14		0x31	//0x188
#define R13		0x32	//0x190
#define R12		0x33	//0x198
#define R11		0x34	//0x1a0
#define R10		0x35	//0x1a8
#define R9		0x36	//0x1b0
#define R8		0x37	//0x1b8
#define RDI		0x38	//0x1c0
#define RSI		0x39	//0x1c8
#define RBP		0x3a	//0x1d0
#define RSP		0x3b	//0x1d8
#define RBX		0x3c	//0x1e0
#define RDX		0x3d	//0x1e8
#define RCX		0x3e	//0x1f0
#define RAX		0x3f	//0x1f8

#define CR2		0x48	//0x240

#define VMCB_GDTR_LIMIT	50	// 0x64h = 50 * 2
#define VMCB_IDTR_LIMIT	66	// 0x84h = 66 * 2

#define CMD_REVIRT		2
#define CMD_KPT_CHANGE	3
#define JS_SMM_ARGUMENT_ADDR	0x49000e00

#define BUF 256
#define _PRINT_FOR_DEBUG_SMIHANDLER_

static void	*free_mem_ptr;
static void *free_mem_end_ptr;
volatile static int started = 0;

/* Check serial input. */
int serial_received(void){
	return inb (DEFAULT_SERIAL_PORT+5) & 1;
}

void print_64bit (u64 value) {
	u32 shifted_value = value >> 32;
	static i = 0;
	print ("smm_state_save area ", i);
	i++;
	print (": ", shifted_value);
	println (" ", (u32)value);
}

void print_64bit_pte (u64 value, int j) {
	u32 shifted_value = value >> 32;
	print ("level3 PTE", j);
	print (": ", shifted_value);
	println (" ", (u32)value);
}

void update_kernel_page_table ()
{
	u64	*level4_pgt_entry;
	u64	*level3_pgt_entry;
	int	j = 0;

	level4_pgt_entry = (u64*)(KERNEL_LEVEL4_PGT);

	// old comment location
	for (j=110; j<111; j++) {
		print("level4_pgt_entry ", j);
		println(":	", (u64)*(level4_pgt_entry + j));
	}

	/*
	for (j=0; j<512; j++) {
		if ( *(level4_pgt_entry + j) == 0) {
			print("level4_pgt_entry ", j);
			println(":	", (u64)*(level4_pgt_entry + j));
			break;
		}
	}*/
	// old comment location end

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	print("level4_pgt_entry plus 0x110 ", 0);
	println(":	", (u64)*(level4_pgt_entry + 0x110));
#endif
	level3_pgt_entry = (u64*)*(level4_pgt_entry + 0x110);
	level3_pgt_entry = (u64*)((u64)level3_pgt_entry & 0xfffff000);	// erase attribute field

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	print("level3_pgt_entry ", 0);
	println(":	", (u64)level3_pgt_entry);
#endif
	/*	
		for (j=0; j<512; j++) {
		print_64bit_pte ((u64)*(level3_pgt_entry + j), j);
		} */

	for (j=0; j< 0x102; j++) {
		if ( *(level3_pgt_entry + j) == 0) {
			print_64bit_pte ((u64)*(level3_pgt_entry + j), j);
			break;
		}
	}

	*(level3_pgt_entry + PDPE_ENTRY_NUM) = 0x40000000 | PDPE_G_BIT | PDPE_PS_BIT | PTE_P_BIT | PTE_RW_BIT; 	// allocate 1Gbyte page table

	print_64bit_pte ((u64)*(level3_pgt_entry + PDPE_ENTRY_NUM), PDPE_ENTRY_NUM);
}

void save_state_save_area_into_guest_vmcb (smm_state_save_area_t *state_save)
{
	u64	*guest_vmcb_pa_location = (u64*)GUEST_VMCB_PA_LOCATION;
	u64	*guest_vmcb_pa = NULL;
	u16 *guest_vmcb_pa_short = NULL;
	u64 msr_value = 0;

	guest_vmcb_pa = (u64*)(*guest_vmcb_pa_location);
	guest_vmcb_pa += VMCB_START_STATE_SAVE_AREA;	// State save area starts at 0x400 of vmcb
	guest_vmcb_pa_short = (u16*)guest_vmcb_pa;

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	println ("Physical address of Guest VMCB state-save area: ", (u64)guest_vmcb_pa);
#endif

	/* save segment registers and descriptor table registers
	 * ES, CS, SS, DS, FS, GS, GDTR, LDTR, IDTR, TR */
	memcpy((void*)guest_vmcb_pa, (void*)state_save, SEG_DESC_TABLE_SIZE);
	*(guest_vmcb_pa + GDTR_SEL_ATTR) = 0;
	*(guest_vmcb_pa + IDTR_SEL_ATTR) = 0;
	*(guest_vmcb_pa_short + VMCB_GDTR_LIMIT) = state_save->gdtr_limit;
	*(guest_vmcb_pa_short + VMCB_IDTR_LIMIT) = state_save->idtr_limit;

	*(guest_vmcb_pa + EFER) = state_save->efer;
	*(guest_vmcb_pa + CR4) = state_save->cr4;
	*(guest_vmcb_pa + CR3) = state_save->cr3;
	*(guest_vmcb_pa + CR0) = state_save->cr0;
	*(guest_vmcb_pa + DR7) = state_save->dr7;
	*(guest_vmcb_pa + DR6) = state_save->dr6;
	*(guest_vmcb_pa + RFLAGS) = state_save->rflags;
	*(guest_vmcb_pa + RIP) = state_save->rip;
	*(guest_vmcb_pa + RSP) = state_save->rsp;
	*(guest_vmcb_pa + RAX) = state_save->rax;

	asm_rdmsr64(STAR_MSR, &msr_value);
	*(guest_vmcb_pa + STAR) = msr_value;
	asm_rdmsr64(LSTAR_MSR, &msr_value);
	*(guest_vmcb_pa + LSTAR) = msr_value; 
	asm_rdmsr64(CSTAR_MSR, &msr_value);
	*(guest_vmcb_pa + CSTAR) = msr_value; 
	asm_rdmsr64(SFMASK_MSR, &msr_value);
	*(guest_vmcb_pa + SFMASK) = msr_value; 
	asm_rdmsr64(KERNELGSBASE_MSR, &msr_value);
	*(guest_vmcb_pa + KERNELGSBASE) = msr_value; 
	asm_rdmsr64(SYSENTER_CS_MSR, &msr_value);
	*(guest_vmcb_pa + SYSENTER_CS) = msr_value; 
	asm_rdmsr64(SYSENTER_ESP_MSR, &msr_value);
	*(guest_vmcb_pa + SYSENTER_ESP) = msr_value; 
	asm_rdmsr64(SYSENTER_EIP_MSR, &msr_value);
	*(guest_vmcb_pa + SYSENTER_EIP) = msr_value; 

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	println ("guest_vmcb_pa's ES: ", (u32)*(guest_vmcb_pa + ES));		// Stopped!!!
	println ("guest_vmcb_pa's CS: ", (u32)*(guest_vmcb_pa + CS));
	println ("guest_vmcb_pa's SS: ", (u32)*(guest_vmcb_pa + SS));
	println ("guest_vmcb_pa's DS: ", (u32)*(guest_vmcb_pa + DS));
	println ("guest_vmcb_pa's FS: ", (u32)*(guest_vmcb_pa + FS));
	println ("guest_vmcb_pa's GS: ", (u32)*(guest_vmcb_pa + GS));
	println ("guest_vmcb_pa's GDTR_LIMIT: ", (u16)*(guest_vmcb_pa_short + VMCB_GDTR_LIMIT));
	println ("guest_vmcb_pa's IDTR_LIMIT: ", (u16)*(guest_vmcb_pa_short + VMCB_IDTR_LIMIT));
	println ("guest_vmcb_pa's GDTR_BASE: ", (u32)*(guest_vmcb_pa + GDTR_BASE));
	println ("guest_vmcb_pa's IDTR_BASE: ", (u32)*(guest_vmcb_pa + IDTR_BASE));
	println ("guest_vmcb_pa's LDTR: ", (u32)*(guest_vmcb_pa + LDTR));
	println ("guest_vmcb_pa's TR: ", (u32)*(guest_vmcb_pa + TR));
	println ("guest_vmcb_pa's EFER: ", (u32)*(guest_vmcb_pa + EFER));
	println ("guest_vmcb_pa's CR4: ", (u32)*(guest_vmcb_pa + CR4));
	println ("guest_vmcb_pa's CR3: ", (u32)*(guest_vmcb_pa + CR3));
	println ("guest_vmcb_pa's CR0: ", (u32)*(guest_vmcb_pa + CR0));
	println ("guest_vmcb_pa's DR7: ", (u32)*(guest_vmcb_pa + DR7));
	println ("guest_vmcb_pa's DR6: ", (u32)*(guest_vmcb_pa + DR6));
	println ("guest_vmcb_pa's RFLAGS: ", (u32)*(guest_vmcb_pa + RFLAGS));
	println ("guest_vmcb_pa's RIP: ", (u32)*(guest_vmcb_pa + RIP));
	println ("guest_vmcb_pa's RSP: ", (u32)*(guest_vmcb_pa + RSP));
	println ("guest_vmcb_pa's RAX: ", (u32)*(guest_vmcb_pa + RAX));
	println ("guest_vmcb_pa's STAR: ", (u32)*(guest_vmcb_pa + STAR));
	println ("guest_vmcb_pa's LSTAR: ", (u32)*(guest_vmcb_pa + LSTAR));
	println ("guest_vmcb_pa's CSTAR: ", (u32)*(guest_vmcb_pa + CSTAR));
	println ("guest_vmcb_pa's SFMASK: ", (u32)*(guest_vmcb_pa + SFMASK));
	println ("guest_vmcb_pa's KERNELGSBASE: ", (u32)*(guest_vmcb_pa + KERNELGSBASE));
	println ("guest_vmcb_pa's SYSENTER_CS: ", (u32)*(guest_vmcb_pa + SYSENTER_CS));
	println ("guest_vmcb_pa's SYSENTER_ESP: ", (u32)*(guest_vmcb_pa + SYSENTER_ESP));
	println ("guest_vmcb_pa's SYSENTER_EIP: ", (u32)*(guest_vmcb_pa + SYSENTER_EIP));
#endif

	// save cr2 register value
	asm volatile( \
			"push %%rax\n\t" \
			"mov %%cr2, %%rax\n\t" \
			"mov %%rax, %0\n\t" \
			"pop %%rax" : "=a" (*(guest_vmcb_pa + CR2)));
}

void save_host_gprs_into_host_state (void)
{
	u64	*host_gprs_state = (u64*)HOST_GPRS_STATE;	// 0x49000b00
	u64	*host_gprs = (u64*)HOST_GPRS;				// 0x49000a18
	u16	*host_gprs_state_short = (u16*)HOST_GPRS_STATE;
	u16 *host_gprs_short = (u16*)HOST_GPRS;
	u64	*host_vmcb_pa = (u64*)HOST_VMCB_PA;
	u16 *host_vmcb_pa_short = NULL;

	host_vmcb_pa += VMCB_START_STATE_SAVE_AREA;
	host_vmcb_pa_short = (u16*)host_vmcb_pa;
#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	println ("address of BitVisor states which will be used in revirt code: ", (u64)host_gprs_state);
	println ("address of host gprs in miscellaneous data area: ", (u64)host_gprs);
#endif

	*(host_gprs_state + HOST_R15) = *(host_gprs + HOST_R15);
	*(host_gprs_state + HOST_R14) = *(host_gprs + HOST_R14);
	*(host_gprs_state + HOST_R13) = *(host_gprs + HOST_R13);
	*(host_gprs_state + HOST_R12) = *(host_gprs + HOST_R12);
	*(host_gprs_state + HOST_R11) = *(host_gprs + HOST_R11);
	*(host_gprs_state + HOST_R10) = *(host_gprs + HOST_R10);
	*(host_gprs_state + HOST_R9) = *(host_gprs + HOST_R9);
	*(host_gprs_state + HOST_R8) = *(host_gprs + HOST_R8);
	*(host_gprs_state + HOST_RDI) = *(host_gprs + HOST_RDI);
	*(host_gprs_state + HOST_RSI) = *(host_gprs + HOST_RSI);
	*(host_gprs_state + HOST_RBP) = *(host_gprs + HOST_RBP);
	*(host_gprs_state + HOST_RSP) = *(host_gprs + HOST_RSP);
	*(host_gprs_state + HOST_RBX) = *(host_gprs + HOST_RBX);
	*(host_gprs_state + HOST_RDX) = *(host_gprs + HOST_RDX);
	*(host_gprs_state + HOST_RCX) = *(host_gprs + HOST_RCX);
	// Save segment selector	// Actually only select is needed
	memcpy((void*)(host_gprs_state + HOST_AFTER_RCX), (void*)(host_vmcb_pa), SEG_SIZE);
	// GDTR
	*(host_gprs_state_short + BITVISOR_STATE_GDTR_LIMIT) = *(host_vmcb_pa_short + VMCB_GDTR_LIMIT);
	*(host_gprs_state + HOST_AFTER_RCX + GDTR_BASE) = *(host_vmcb_pa + GDTR_BASE);
	// IDTR
	*(host_gprs_state_short + BITVISOR_STATE_IDTR_LIMIT) = *(host_vmcb_pa_short + VMCB_IDTR_LIMIT);
	*(host_gprs_state + HOST_AFTER_RCX + IDTR_BASE) = *(host_vmcb_pa + IDTR_BASE);
	// EFER
	*(host_gprs_state + HOST_AFTER_RCX + EFER) = *(host_vmcb_pa + EFER);
	*(host_gprs_state + HOST_AFTER_RCX + CR4) = *(host_vmcb_pa + CR4);
	*(host_gprs_state + HOST_AFTER_RCX + CR3) = *(host_vmcb_pa + CR3);
	*(host_gprs_state + HOST_AFTER_RCX + CR0) = *(host_vmcb_pa + CR0);
	//*(host_gprs_state + HOST_AFTER_RCX + DR7) = *(host_vmcb_pa + DR7);
	//*(host_gprs_state + HOST_AFTER_RCX + DR6) = *(host_vmcb_pa + DR6);
	*(host_gprs_state + HOST_AFTER_RCX + RFLAGS) = *(host_vmcb_pa + RFLAGS);
	*(host_gprs_state + HOST_AFTER_RCX + RIP) = *(host_vmcb_pa + RIP);
	*(host_gprs_state + HOST_AFTER_RCX + RSP) = *(host_vmcb_pa + RSP);
	*(host_gprs_state + HOST_AFTER_RCX + RAX) = *(host_vmcb_pa + RAX);

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	println ("host state [in 0x49000a18] CR3: ", (u64)*(host_vmcb_pa + CR3));
	println ("host state [in 0x49000b00] CR3: ", (u64)*(host_gprs_state + HOST_AFTER_RCX + CR3));
	println ("host state [in 0x49000b00] R15: ", (u32)*(host_gprs_state + HOST_R15));
	println ("host state [in 0x49000b00] R14: ", (u32)*(host_gprs_state + HOST_R14));
	println ("host state [in 0x49000b00] R13: ", (u32)*(host_gprs_state + HOST_R13));
	println ("host state [in 0x49000b00] R12: ", (u32)*(host_gprs_state + HOST_R12));
	println ("host state [in 0x49000b00] R11: ", (u32)*(host_gprs_state + HOST_R11));
	println ("host state [in 0x49000b00] R10: ", (u32)*(host_gprs_state + HOST_R10));
	println ("host state [in 0x49000b00] R9: ", (u32)*(host_gprs_state + HOST_R9));
	println ("host state [in 0x49000b00] R8: ", (u32)*(host_gprs_state + HOST_R8));
	println ("host state [in 0x49000b00] RDI: ", (u32)*(host_gprs_state + HOST_RDI));
	println ("host state [in 0x49000b00] RSI: ", (u32)*(host_gprs_state + HOST_RSI));
	println ("host state [in 0x49000b00] RBP: ", (u32)*(host_gprs_state + HOST_RBP));
	println ("host state [in 0x49000b00] RSP: ", (u32)*(host_gprs_state + HOST_RSP));
	println ("host state [in 0x49000b00] RBX: ", (u32)*(host_gprs_state + HOST_RBX));
	println ("host state [in 0x49000b00] RDX: ", (u32)*(host_gprs_state + HOST_RDX));
	println ("host state [in 0x49000b00] RCX: ", (u32)*(host_gprs_state + HOST_RCX));
	println ("host state [in 0x49000b00] ES: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + ES));
	println ("host state [in 0x49000b00] CS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + CS));
	println ("host state [in 0x49000b00] SS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + SS));
	println ("host state [in 0x49000b00] DS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + DS));
	println ("host state [in 0x49000b00] FS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + FS));
	println ("host state [in 0x49000b00] GS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + GS));
	println ("host state [in 0x49000b00] GDTR_LIMIT: ", (u16)*(host_gprs_state_short + BITVISOR_STATE_GDTR_LIMIT));	
	println ("host state [in 0x49000b00] IDTR_LIMIT: ", (u16)*(host_gprs_state_short + BITVISOR_STATE_IDTR_LIMIT));
	println ("host state [in 0x49000b00] GDTR: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + GDTR_BASE));
	println ("host state [in 0x49000b00] IDTR: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + IDTR_BASE));
	println ("host state [in 0x49000b00] EFER: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + EFER));
	println ("host state [in 0x49000b00] CR4: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + CR4));
	println ("host state [in 0x49000b00] CR3: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + CR3));
	println ("host state [in 0x49000b00] CR0: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + CR0));
	//println ("host state [in 0x49000b00] DR7: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + DR7));
	//println ("host state [in 0x49000b00] DR6: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + DR6));
	println ("host state [in 0x49000b00] RFLAGS: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + RFLAGS));
	println ("host state [in 0x49000b00] RIP: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + RIP));
	println ("host state [in 0x49000b00] RSP: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + RSP));
	println ("host state [in 0x49000b00] RAX: ", (u32)*(host_gprs_state + HOST_AFTER_RCX + RAX));
#endif
}

void smi_handler(void)
{
	size_t 					length;
	int 					ret;
	unsigned long 			state_save_addr = 0;
	smm_state_save_area_t 	*state_save;
	smi_request_t 			*req	= (smi_request_t *)(SMM_ARGUMENT_ADDR);
	int						i = 0;	// only use core 0
	int						revirt_switch_code_size = 0;
	u64						*level4_empty_entry;
	u64						*page_table;
	u64						level4_empty_entry_number = -1;
	u64						*timer_unset_value = (u64*)0x49000a90;
	//u32						*smm_argument = NULL;
	char serial_input;
	/*0x0f, 0x0b*/	// invalid opcode


	char 					revirt_switch_code[] = {
		0xb0, 0x00, 0x66, 0xba, 0xf1, 0x03,
		0xee, 0xb0, 0x03, 0x66, 0xba, 0xf3, 0x03, 0xee, 0xb0, 0x00, 0x66, 0xba,
		0xf2, 0x03, 0xee, 0xb0, 0x03, 0x66, 0xba, 0xf4, 0x03, 0xee, 0x66, 0xba,
		0xf3, 0x03, 0xec, 0x88, 0xc4, 0x0c, 0x80, 0x66, 0xba, 0xf3, 0x03, 0xee,
		0xb0, 0x01, 0x66, 0xba, 0xf0, 0x03, 0xee, 0xb0, 0x00, 0x66, 0xba, 0xf1,
		0x03, 0xee, 0x88, 0xe0, 0x24, 0x7f, 0x66, 0xba, 0xf3, 0x03, 0xee, 0x48,
		0xbf, 0x00, 0x0b, 0x00, 0x49, 0x40, 0x88, 0xff, 0xff, 0x41, 0x0f, 0x20,
		0xe1, 0x49, 0x81, 0xe1, 0x7f, 0xff, 0xff, 0xff, 0x41, 0x0f, 0x22, 0xe1,
		0x0f, 0x01, 0x97, 0xde, 0x00, 0x00, 0x00, 0x0f, 0x01, 0x9f, 0xfe, 0x00,
		0x00, 0x00, 0x4c, 0x8b, 0x97, 0xe8, 0x01, 0x00, 0x00, 0x49, 0x81, 0xca,
		0x00, 0x00, 0x20, 0x00, 0x41, 0x52, 0x9d, 0x4c, 0x8b, 0x8f, 0xc8, 0x01,
		0x00, 0x00, 0x41, 0x0f, 0x22, 0xd9, 0x48, 0xc7, 0xc0, 0x00, 0x10, 0x00,
		0x49, 0x0f, 0x01, 0xda, 0x8e, 0x47, 0x78, 0x8e, 0x9f, 0xa8, 0x00, 0x00,
		0x00, 0x48, 0x8b, 0x9f, 0xd8, 0x01, 0x00, 0x00, 0x0f, 0x23, 0xfb, 0x48,
		0x8b, 0x9f, 0xe0, 0x01, 0x00, 0x00, 0x0f, 0x23, 0xf3, 0xb9, 0x80, 0x00,
		0x00, 0xc0, 0x8b, 0x87, 0x48, 0x01, 0x00, 0x00, 0x8b, 0x97, 0x4c, 0x01,
		0x00, 0x00, 0x0f, 0x30, 0x48, 0x8b, 0x9f, 0xd0, 0x01, 0x00, 0x00, 0x0f,
		0x22, 0xc3, 0x48, 0x8b, 0x9f, 0xc0, 0x01, 0x00, 0x00, 0x0f, 0x22, 0xe3,
		0x48, 0x0f, 0xb2, 0x9f, 0x98, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x67, 0x58,
		0x48, 0x89, 0xe5, 0x48, 0x31, 0xdb, 0x66, 0x8b, 0x9f, 0x88, 0x00, 0x00,
		0x00, 0x53, 0xff, 0xb7, 0xf0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0x87, 0x70,
		0x02, 0x00, 0x00, 0x48, 0x8b, 0x4f, 0x70, 0x48, 0x8b, 0x57, 0x68, 0x48,
		0x8b, 0x5f, 0x60, 0x48, 0x8b, 0x6f, 0x50, 0x48, 0x8b, 0x77, 0x48, 0x4c,
		0x8b, 0x47, 0x38, 0x4c, 0x8b, 0x4f, 0x30, 0x4c, 0x8b, 0x57, 0x28, 0x4c,
		0x8b, 0x5f, 0x20, 0x4c, 0x8b, 0x67, 0x18, 0x4c, 0x8b, 0x6f, 0x10, 0x4c,
		0x8b, 0x77, 0x08, 0x4c, 0x8b, 0x3f, 0x48, 0x8b, 0x7f, 0x40, 0x48, 0xcb};	// without serial print

	/*	char 					revirt_switch_code[] = {
		0xb0, 0x00, 0x66, 0xba, 0xf1, 0x03,
		0xee, 0xb0, 0x03, 0x66, 0xba, 0xf3, 0x03, 0xee, 0xb0, 0x00, 0x66, 0xba,
		0xf2, 0x03, 0xee, 0xb0, 0x03, 0x66, 0xba, 0xf4, 0x03, 0xee, 0x66, 0xba,
		0xf3, 0x03, 0xec, 0x88, 0xc4, 0x0c, 0x80, 0x66, 0xba, 0xf3, 0x03, 0xee,
		0xb0, 0x01, 0x66, 0xba, 0xf0, 0x03, 0xee, 0xb0, 0x00, 0x66, 0xba, 0xf1,
		0x03, 0xee, 0x88, 0xe0, 0x24, 0x7f, 0x66, 0xba, 0xf3, 0x03, 0xee, 0x49,
		0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x41, 0x66, 0xba, 0xf8, 0x03, 0xee,
		0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74,
		0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x49, 0x89, 0xc4, 0x49, 0x89,
		0xd5, 0xb0, 0x42, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05,
		0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c, 0x89, 0xe0,
		0x4c, 0x89, 0xea, 0x48, 0xbf, 0x00, 0x0b, 0x00, 0x49, 0x40, 0x88, 0xff,
		0xff, 0x41, 0x0f, 0x20, 0xe1, 0x49, 0x81, 0xe1, 0x7f, 0xff, 0xff, 0xff,
		0x41, 0x0f, 0x22, 0xe1, 0x0f, 0x01, 0x97, 0xde, 0x00, 0x00, 0x00, 0x0f,
		0x01, 0x9f, 0xfe, 0x00, 0x00, 0x00, 0x4c, 0x8b, 0x97, 0xe8, 0x01, 0x00,
		0x00, 0x49, 0x81, 0xca, 0x00, 0x00, 0x20, 0x00, 0x41, 0x52, 0x9d, 0x49,
		0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x43, 0x66, 0xba, 0xf8, 0x03, 0xee,
		0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74,
		0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x4c, 0x8b, 0x8f, 0xc8, 0x01,
		0x00, 0x00, 0x41, 0x0f, 0x22, 0xd9, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5,
		0xb0, 0x44, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3,
		0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c,
		0x89, 0xea, 0x48, 0xc7, 0xc0, 0x00, 0x10, 0x00, 0x49, 0x0f, 0x01, 0xda,
		0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x45, 0x66, 0xba, 0xf8, 0x03,
		0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00,
		0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x8e, 0x47, 0x78, 0x8e,
		0x9f, 0xa8, 0x00, 0x00, 0x00, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0,
		0x46, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90,
		0xec, 0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89,
		0xea, 0x48, 0x8b, 0x9f, 0xd8, 0x01, 0x00, 0x00, 0x0f, 0x23, 0xfb, 0x48,
		0x8b, 0x9f, 0xe0, 0x01, 0x00, 0x00, 0x0f, 0x23, 0xf3, 0x49, 0x89, 0xc4,
		0x49, 0x89, 0xd5, 0xb0, 0x47, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83,
		0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c,
		0x89, 0xe0, 0x4c, 0x89, 0xea, 0xb9, 0x80, 0x00, 0x00, 0xc0, 0x8b, 0x87,
		0x48, 0x01, 0x00, 0x00, 0x8b, 0x97, 0x4c, 0x01, 0x00, 0x00, 0x0f, 0x30,
		0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x48, 0x66, 0xba, 0xf8, 0x03,
		0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00,
		0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0x8b, 0x9f, 0xd0,
		0x01, 0x00, 0x00, 0x0f, 0x22, 0xc3, 0x48, 0x8b, 0x9f, 0xc0, 0x01, 0x00,
		0x00, 0x0f, 0x22, 0xe3, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x49,
		0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec,
		0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea,
		0x48, 0x0f, 0xb2, 0x9f, 0x98, 0x00, 0x00, 0x00, 0x49, 0x89, 0xc4, 0x49,
		0x89, 0xd5, 0xb0, 0x4a, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2,
		0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74, 0xf7, 0x4c, 0x89,
		0xe0, 0x4c, 0x89, 0xea, 0x48, 0x8b, 0x67, 0x58, 0x48, 0x89, 0xe5, 0x49,
		0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x4b, 0x66, 0xba, 0xf8, 0x03, 0xee,
		0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00, 0x74,
		0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0x31, 0xdb, 0x66, 0x8b,
		0x9f, 0x88, 0x00, 0x00, 0x00, 0x53, 0xff, 0xb7, 0xf0, 0x01, 0x00, 0x00,
		0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x4c, 0x66, 0xba, 0xf8, 0x03,
		0xee, 0x66, 0x83, 0xc2, 0x05, 0xf3, 0x90, 0xec, 0x24, 0x20, 0x3c, 0x00,
		0x74, 0xf7, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0x8b, 0x87, 0x70,
		0x02, 0x00, 0x00, 0x48, 0x8b, 0x4f, 0x70, 0x48, 0x8b, 0x57, 0x68, 0x48,
		0x8b, 0x5f, 0x60, 0x48, 0x8b, 0x6f, 0x50, 0x48, 0x8b, 0x77, 0x48, 0x4c,
		0x8b, 0x47, 0x38, 0x4c, 0x8b, 0x4f, 0x30, 0x4c, 0x8b, 0x57, 0x28, 0x4c,
		0x8b, 0x5f, 0x20, 0x4c, 0x8b, 0x67, 0x18, 0x4c, 0x8b, 0x6f, 0x10, 0x4c,
		0x8b, 0x77, 0x08, 0x4c, 0x8b, 0x3f, 0x48, 0x8b, 0x7f, 0x40, 0x48, 0xcb
		}; */

	state_save_addr = SMM_TSEG_ADDR + SMM_SAVE_STATE_OFFSET + (SMM_TSEG_OFFSET*i);
	state_save = (smm_state_save_area_t *)state_save_addr;
	*timer_unset_value = 0;

	if (started == 0)
	{
		//#ifndef NOT_DEBUG
		serial_init(DEFAULT_SERIAL_PORT,DEFAULT_BAUD);	// Serial initialization
		//#endif
		free_mem_ptr 		= (void *)(HEAP_START) ;
		free_mem_end_ptr 	= (void *)(HEAP_START + HEAP_SIZE);

		init_page_allocator((u64)free_mem_ptr, (u64)free_mem_end_ptr);
		init_free_list();
		started = 1;
	}

	//println("Hello World ", req->param_test.addr);
	//smm_argument = (u32*)JS_SMM_ARGUMENT_ADDR;
	//req->command = *(smm_argument);
#ifdef _SERIAL_TEST_
	//*(smm_argument) = 0;
	req->command = CMD_TEST;
#endif

	//*(smm_argument) = CMD_RESTART;
	if (req != NULL)
	{
		switch (req->command)
		{
			case CMD_KPT_CHANGE :	// change kernel page table & locate revirt switch code
				println("CMD KPT CHANGE, number: ", CMD_KPT_CHANGE);

				revirt_switch_code_size = sizeof(revirt_switch_code) / sizeof(char);
				update_kernel_page_table();
				/* locate revirt switch code */
				memcpy ((void*)REVIRT_SWITCH_CODE, (void*)revirt_switch_code, revirt_switch_code_size);
				//*(smm_argument) = 0;

				break;

			case CMD_TEST :	// == 99	// SERIAL TEST with it
				println("In CME TEST ", 0);

				// Serial read.
				while (serial_received() != 0){		// while loop is added for checking serial input data. Ref) OSDev
					serial_input = inb(DEFAULT_SERIAL_PORT);
				}
				println("SERIAL received: ", serial_input);

				break;

			case CMD_REVIRT:	// revirt
#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
				println("CMD_REVIRT", 0);
#endif
				timer_auto_enable (6, MAX_CALL);
				revirt_switch_code_size = sizeof(revirt_switch_code) / sizeof(char);

				if ((state_save->cs_attribute.dpl & 0x3) == 0) {
					//println("cs_selecotr's RPL is zero ", state_save->cs_selector);
					if ((state_save->rflags & 0x200) == 0) {
						//println("RFLAGS.IF bit is also ", state_save->rflags);
						/* To jmp 0xFFFF880049000400 when SMM out */
						//update_kernel_page_table();

						/* locate revirt switch code */
						memcpy ((void*)REVIRT_SWITCH_CODE, (void*)revirt_switch_code, revirt_switch_code_size);

						/* save state_save_area to STATE_SAVE_AREA_IN_SHARED_AREA */
						memcpy ((void*)STATE_SAVE_AREA_IN_SHARED_AREA, (void*)state_save, STATE_SAVE_AREA_SIZE);

#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
						print_64bit (state_save->r15);
						print_64bit (state_save->r14);
						print_64bit (state_save->r13);
						print_64bit (state_save->r12);
						print_64bit (state_save->r11);
						print_64bit (state_save->r10);
						print_64bit (state_save->r9);
						print_64bit (state_save->r8);
						print_64bit (state_save->rdi);
						print_64bit (state_save->rsi);
						print_64bit (state_save->rbp);
						print_64bit (state_save->rbx);
						print_64bit (state_save->rdx);
						print_64bit (state_save->rcx);
#endif
						save_host_gprs_into_host_state();
						save_state_save_area_into_guest_vmcb(state_save);
						/* modify rip  */
						state_save->rip = 0xffff884049000400;	// jmp to physical addr 0x49000400 using kernel direct mapping
#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
						println("rip is changed", 0);
#endif
						//timer_unset ();
						*(timer_unset_value) = 1;
						timer_auto_enable (6, 0);
						*(timer_unset_value) = 0;

						//println("timer is unset", 0);
					}
				}
				break;			
			default:
				println("default case", 0);
		}
	}
	/* code */
#ifdef _PRINT_FOR_DEBUG_SMIHANDLER_
	println ("switch statement ended ", 0);
#endif
	clear_smi_status();
	smi_set_eos();
}
