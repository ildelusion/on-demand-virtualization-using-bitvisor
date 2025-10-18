/*
 * This revirt.c file is appended in 2015-10-15
 * Author: Jaeseong Im, Jongyul Kim of KAIST
 * This source code is for revirtualizing BitVisor
 */

#include "svm_revirt.h"

/* Print VMCB. For DEBUG. By JYKIM. */
	void
print_vmcb(long *vmcb_vaddr)
{
	int i=0, iter = 0;
	long value = 0;
	//iter = sizeof(struct vmcb) / sizeof(long);
	iter = 0xe0;
	printf("[PRINT VMCB] sizeof vmcb:%d, sizeof long:%d, iter:%d\n",
			sizeof(struct vmcb), sizeof(long), iter);

	for (i=0; i<iter; i++){
		value = (long)*(vmcb_vaddr+i);
		printf("[PRINT VMCB] (%016lx) : %04x %04x %04x %04x \ti:%d\t%llx\n",
				vmcb_vaddr+i, 
				(value >> 0x30) & 0xffff,
				(value >> 0x20) & 0xffff,
				(value >> 0x10) & 0xffff,
				value & 0xffff,
				i, value);
	}
}

/* Save u64 value to memory. */
	void
save_u64 (long *shared_area_vaddr, u64 shared_area_offset, u64 value)
{
	long *target_addr = 0;
	target_addr = shared_area_vaddr + shared_area_offset/sizeof(long*);
	*target_addr = value;

#ifdef _PRINT_FOR_REVIT_
	printf("[REVIRT] value (%llx) is saved at VA(%llx). SHARED_AREA_OFFSET:%llx\n",
			*target_addr,
			target_addr,
			shared_area_offset);	
#endif
}

/* Save physical address of guest VMCB. */
	void
save_gvmcb_pa (long *shared_area_vaddr)
{
	// Map the memory of which physical address is SHARED_AREA_MISC_GVMCB_PA.
	// And save gVMCB's PA.
	save_u64(shared_area_vaddr, SHARED_AREA_OFFSET_MISC_GVMCB_PA, 
			current->u.svm.vi.vmcb_phys);
}
