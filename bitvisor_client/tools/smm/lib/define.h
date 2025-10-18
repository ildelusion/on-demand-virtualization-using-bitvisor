#ifndef SMM_DEFINE_H
#define SMM_DEFINE_H

#define MSR_HWCR			0xc0010015
#define MSR_SMMBASE			0xc0010111
#define MSR_SMMADDR			0xc0010112
#define MSR_SMMMASK			0xc0010113
#define MSR_SMMTRIG_IO		0xc0010056
#define MSR_SMITRAP_CONTROL	0xc0010054
#define MSR_FIX_MTRR		0x00000259
#define MSR_SYS_CFG			0xc0010010
#define MSR_EFER			0xc0000080


#define RFLAGS_IF			(1 << 9)
#define GDT_ENTRY_32BIT_CS	0x8
#define GDT_ENTRY_32BIT_DS	0x10
#define GDT_ENTRY_64BIT_CS	0x18
#define GDT_ENTRY_64BIT_DS	0x20

#define	START_64BIT_ENTRY	0x400
#define _EFER_LME       8  /* Long mode enable */

#define X86_CR0_PE              0x00000001 /* Enable Protected Mode    (RW) */
#define X86_CR0_PG              0x80000000 /* Paging                   (RW) */

#define MTRR_FIX_DRAM_EN	(1 << 19)
#define MTRR_FIX_VAL		0x1010101010101010ULL

#define PMIO_COMMAND			0xcd6
#define PMIO_DATA				0xcd7
#define PMIO_EOS				0x10
#define PMIO_SMICOMMAND			0x2a
#define	PMIO_SMISTATUS			0x2b
#define	PMIO_SMMENABLE			0x53
#define	PMIO_SMICOMENABLE		0x0e
#define	PMIO_SMIRESULT			0x0f
#define	PMIO_PIO				0x14
#define	PMIO_WAKESMI			0x04
#define	PMIO_WAKEIRQ			0x03
#define	PMIO_WAKESMI_STATUS		0x07
#define	PMIO_WAKEIRQ_STATUS		0x06
#define	PMIO_WAKESMI_SERI		0x10
#define	PMIO_TIMER2				0x12
#define	PMIO_TIMER2_RET			0x13
#define	PMIO_TIMER2_ENABLE_BIT	0x4
#define	PMIO_MISCCONTROL		0x0
#define	PMIO_MISCSTATUS			0x1
#define	PMIO_SMMENABLE_BIT		0x8
#define	PMIO_SMICOMENABLE_BIT	0x4
#define	PMIO_EOS_BIT			0x1
#define	PMIO_TIMER_BIT			(0x1 << 2)


#define CPU					6
#define HEAP_START			(SMM_TSEG_ADDR + 0x1000000)
#define HEAP_SIZE			(0x3000000)
#define SMM_LOCK_BIT		(1 << 0)
#define SMM_ASEG_ADDR		0xa8000
#define SMM_OSEG_ADDR		0xaff00000
#define SMM_TSEG_ADDR		0x40000000
#define SMM_TSEG_OFFSET		0x1000
#define SMM_START_ADDR		(SMM_TSEG_ADDR + ((CPU-1) * SMM_TSEG_OFFSET))
#define SMM_ASEG_END		0xbffff
#define SMM_TSEG_ZERO_BIT	0xfffff
#define SMM_ARGUMENT_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x00000)
#define SMM_CORE_SET_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x10000)
#define SMM_GHOST_ARGUMENT_ADDR	(SMM_TSEG_ADDR + 0x4000000 + 0x20000)
#define SMM_GHOST_LOCK_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x28000)
#define SMM_GHOST_ADDR			(SMM_TSEG_ADDR + 0x4000000 + 0x30000)
#define SMM_GHOST_NPT_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x200000)
#define SMM_GHOST_GPT_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x400000)
#define SMM_GHOST_VM_ADDR		(SMM_TSEG_ADDR + 0x4000000 + 0x600000)
#define SMM_GHOST_VM_LOCK_OFFSET 0xd000
#define SMM_GHOST_VM_LOCK_ADDR	(SMM_GHOST_VM_ADDR + SMM_GHOST_VM_LOCK_OFFSET)
#define SMM_GHOST_VM_ARG_OFFSET 0xf000
#define SMM_GHOST_VM_ARG_RIP_OFFSET 0x100
#define SMM_GHOST_VM_ARG_ADDR	(SMM_GHOST_VM_ADDR + SMM_GHOST_VM_ARG_OFFSET)
#define SMM_GHOST_COMMAND_ALLOC 1
#define SMM_GHOST_COMMAND_RELEASE 2
#define TEST_ADDR			(SMM_TSEG_ADDR + 0x5600)
// UnProtected
//#define SMM_TSEG_MASK_BASE	(0xfffffff8) 
// protected
#define SMM_TSEG_MASK_BASE	(0xfffffc00)
#define SMM_TSEG_MASK_SIZE	(16)
#define SMM_ASEG_VALID		(1 << 0)
#define SMM_ASEG_CLOSE		(1 << 2)
#define SMM_TSEG_VALID		(1 << 1)
#define SMM_TSEG_CLOSE		(1 << 3)
#define SMM_TSEG_DRAM		(4 << 12)
#define SMM_TSEG_SHIFT		17
#define SMM_TRAP_ENABLE		0x8002
#define SMM_TSEG_MASK		0xffff00000001ffff
#define SMM_FILE_PATH		"./smmHandler"
#define SMI_FILE_PATH		"./smihandler"
#define GHOST_FILE_PATH		"./ghost"
#define GHOST_VM_FILE_PATH	"./ghost_vm"
#define SMI_SIMPLE_FILE_PATH "./smihandler_simple"
#define SMM_HANDLER_OFFSET	0x400
#define SMM_ADDR_START		0xa8000
//#define SMM_ADDR_START		0xaff00000
#define SMM_TSEG_SIZE		0x200000
//#define SMM_STACK_START_T	(SMM_ADDR_START + SMM_TSEG_SIZE - SMM_HANDLER_OFFSET)
//#define SMM_STACK_START		(SMM_ADDR_START + SMM_TSEG_SIZE - SMM_HANDLER_OFFSET)
//#define SMM_REAL_START		(SMM_STACK_START + 0x1000)
//#define SMMBASE_TEMP 0x
#define SMM_SAVE_STATE_OFFSET	0xfe00	

#define MEMDEVICE "/dev/mem"
#define MAPPEDAREASIZE (0x1ffff)    /* this is the size of the mapped SMRAM area (in bytes */ 
#define TSEG_MAP_SIZE (0xfffff)    /* this is the size of the mapped SMRAM area (in bytes */          


#define LAPIC_ID    0xfee00020

#define SMMOP_SMM_ENTER         	0x0
#define SMMOP_CREATE_DOM        	0x1
#define SMMOP_DELETE_DOM        	0x2
#define SMMOP_PAGE_MAP          	0x3
#define SMMOP_PAGE_MAP_SUPER    	0x4
#define SMMOP_PAGE_UNMAP        	0x5
#define SMMOP_PAGE_UNMAP_SUPER  	0x6
#define SMMOP_CONTEXT_SAVE      	0x7
#define SMMOP_CONTEXT_RESTORE   	0x8
#define SMMOP_ACCOUNT_VCPU   		0x9
#define SMMOP_ACCOUNT_MEM   		0xa
#define SMMOP_ACCOUNT_CACHE   		0xb
#define SMMOP_PRINT_ACCOUNT_VCPU   	0xc
#define SMMOP_GHOST_VCPU   			0xd
#define SMMOP_GHOST_VCPU_RELEASE	0xe
#define SMMOP_ACCOUNT_RESET   		0xf
#define SMMOP_CREDIT_PRINT   		0x10
#define SMMOP_CREDIT_RESET   		0x11
#define SMMOP_FORCE_SCHEDULE		0x12
#define SMMOP_CREDIT_CHECK   		0x13
#define SMMOP_VICTIM_RUN   			0x14
#define SMMOP_PRINT_ALL_ACCOUNT_VCPU 0x15
#define SMMOP_ACCOUNT_VCPU_SAVE		0x16
#define SMMOP_CREATE_VMCB        	0x17
#define SMMOP_DELETE_VMCB        	0x18

#define DG_CORE					0		// DELEGATION CORE
#define HOST_STATE				0
#define MAX_SLOTS				1000
#define MAX_CALL				MAX_SLOTS

#define XEN_DIRECT_VIRT_START	0xffff830000000000	

#endif
