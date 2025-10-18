#include "svm_devirt.h"
#include "svm_revirt.h"
#include "svm_asm.h"
#include "vmmcall_devirt.h"

#define _PRINT_FOR_DEBUG_DEVIRT_
#define _PRINT_FOR_DEBUG_THREAD_
#define	SMM_ARGUMENT_OFFSET	0x1000e00	// 0x48000000(share_area) + 0x1000e00 = 0x49000e00

bool before_devirt;
static u64 shared_area_addr;

void svm_devirt (long *shared_area_vaddr)
{
	void *mem = NULL;
	long *mem_long = NULL;
	short *mem_short = NULL;
	long test_long = 0;
	long mem_dump = NULL;

	long *g_page_mem = NULL;
	long *h_page_mem = NULL;	// virtual addr of host cr3

	void *g_level4_table_base_address = NULL;
	long *g_level4_empty_entry = NULL;
	long *PDP_alloc_address = NULL;
	long *PD_alloc_address = NULL;
	long *PT_alloc_address = NULL;

	void *h_level4_table_base_address = NULL;
	long *h_level4_empty_entry = NULL;
	long *h_PDP_alloc_address = NULL;
	long *h_PD_alloc_address = NULL;
	long *h_PT_alloc_address = NULL;

	int switch_code_size = 0;
	long *guest_cr3_backup = NULL;
	long *empty_entry_num_backup = NULL;

	int level1_entry_num = 0;
	int level2_entry_num = 0;
	int level3_entry_num = 0;

	long i = 0;
	u64 efer = 0;
	ulong rflags = 0;
	ulong cr0 = 0, cr3 = 0, cr4 = 0, cr8 = 0;
	ulong rsp = 0;
	u16 cs = 0, ss = 0, es = 0, ds = 0, fs = 0, gs = 0, tr = 0, ldtr = 0;
	long *kernel_va = NULL;

	char switch_code[] = {
		0x0f, 0x01, 0x97, 0xde, 0x01, 0x00,
		0x00, 0x0f, 0x01, 0x9f, 0x0e, 0x02, 0x00, 0x00, 0x4c, 0x8b, 0x97, 0x88,
		0x00, 0x00, 0x00, 0x49, 0x81, 0xe2, 0xff, 0xfd, 0xff, 0xff, 0x49, 0x81,
		0xca, 0x00, 0x00, 0x20, 0x00, 0x41, 0x52, 0x9d, 0x0f, 0x22, 0xda, 0x0f,
		0x01, 0xda, 0x8e, 0x9f, 0x38, 0x01, 0x00, 0x00, 0x8e, 0x87, 0x58, 0x01,
		0x00, 0x00, 0x48, 0x8b, 0x9f, 0xb8, 0x00, 0x00, 0x00, 0x0f, 0x23, 0xf3,
		0x48, 0x8b, 0x9f, 0xc0, 0x00, 0x00, 0x00, 0x0f, 0x23, 0xfb, 0xb9, 0x80,
		0x00, 0x00, 0xc0, 0x8b, 0x87, 0x10, 0x01, 0x00, 0x00, 0x8b, 0x97, 0x14,
		0x01, 0x00, 0x00, 0x0f, 0x30, 0x48, 0x8b, 0x9f, 0x98, 0x00, 0x00, 0x00,
		0x0f, 0x22, 0xc3, 0x48, 0x8b, 0x9f, 0xb0, 0x00, 0x00, 0x00, 0x0f, 0x22,
		0xe3, 0x48, 0x8b, 0x5f, 0x20, 0x0f, 0x22, 0xd3, 0x48, 0xc7, 0xc2, 0x00,
		0x00, 0x00, 0x00, 0x48, 0x89, 0x97, 0x10, 0x03, 0x00, 0x00, 0x48, 0x89,
		0x97, 0x18, 0x03, 0x00, 0x00, 0x48, 0x0f, 0xb2, 0x97, 0x10, 0x03, 0x00,
		0x00, 0x48, 0x8b, 0xa7, 0x80, 0x00, 0x00, 0x00, 0x48, 0x89, 0xe5, 0xff,
		0xb7, 0x18, 0x01, 0x00, 0x00, 0xff, 0xb7, 0x90, 0x00, 0x00, 0x00, 0x48,
		0x8b, 0x07, 0x48, 0x8b, 0x4f, 0x08, 0x48, 0x8b, 0x57, 0x10, 0x48, 0x8b,
		0x5f, 0x18, 0x48, 0x8b, 0x6f, 0x28, 0x48, 0x8b, 0x77, 0x30, 0x4c, 0x8b,
		0x47, 0x40, 0x4c, 0x8b, 0x4f, 0x48, 0x4c, 0x8b, 0x57, 0x50, 0x4c, 0x8b,
		0x5f, 0x58, 0x4c, 0x8b, 0x67, 0x60, 0x4c, 0x8b, 0x6f, 0x68, 0x4c, 0x8b,
		0x77, 0x70, 0x4c, 0x8b, 0x7f, 0x78, 0x48, 0x8b, 0x7f, 0x38, 0x48, 0xcb};
	/*
	   char switch_code[] = {0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5,
	   0xb0, 0x42, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec,
	   0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea,
	   0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x44, 0x66, 0xba, 0xf8, 0x03,
	   0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9,
	   0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x0f, 0x01, 0x97, 0xde, 0x01, 0x00,
	   0x00, 0x0f, 0x01, 0x9f, 0x0e, 0x02, 0x00, 0x00, 0x49, 0x89, 0xc4, 0x49,
	   0x89, 0xd5, 0xb0, 0x4a, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2,
	   0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c,
	   0x89, 0xea, 0x4c, 0x8b, 0x97, 0x88, 0x00, 0x00, 0x00, 0x49, 0x81, 0xe2,
	   0xff, 0xfd, 0xff, 0xff, 0x49, 0x81, 0xca, 0x00, 0x00, 0x20, 0x00, 0x41,
	   0x52, 0x9d, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x43, 0x66, 0xba,
	   0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00,
	   0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x0f, 0x22, 0xda, 0x49,
	   0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x4c, 0x66, 0xba, 0xf8, 0x03, 0xee,
	   0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c,
	   0x89, 0xe0, 0x4c, 0x89, 0xea, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0,
	   0x4d, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24,
	   0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x49,
	   0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x4b, 0x66, 0xba, 0xf8, 0x03, 0xee,
	   0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c,
	   0x89, 0xe0, 0x4c, 0x89, 0xea, 0x0f, 0x01, 0xda, 0x8e, 0x9f, 0x38, 0x01,
	   0x00, 0x00, 0x8e, 0x87, 0x58, 0x01, 0x00, 0x00, 0x49, 0x89, 0xc4, 0x49,
	   0x89, 0xd5, 0xb0, 0x45, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2,
	   0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c,
	   0x89, 0xea, 0x48, 0x8b, 0x9f, 0xb8, 0x00, 0x00, 0x00, 0x0f, 0x23, 0xf3,
	   0x48, 0x8b, 0x9f, 0xc0, 0x00, 0x00, 0x00, 0x0f, 0x23, 0xfb, 0xb9, 0x80,
	   0x00, 0x00, 0xc0, 0x8b, 0x87, 0x10, 0x01, 0x00, 0x00, 0x8b, 0x97, 0x14,
	   0x01, 0x00, 0x00, 0x0f, 0x30, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0,
	   0x46, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24,
	   0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48,
	   0x8b, 0x9f, 0x98, 0x00, 0x00, 0x00, 0x0f, 0x22, 0xc3, 0x48, 0x8b, 0x9f,
	   0xb0, 0x00, 0x00, 0x00, 0x0f, 0x22, 0xe3, 0x48, 0x8b, 0x5f, 0x20, 0x0f,
	   0x22, 0xd3, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x47, 0x66, 0xba,
	   0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00,
	   0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0xc7, 0xc2, 0x00,
	   0x00, 0x00, 0x00, 0x48, 0x89, 0x97, 0x10, 0x03, 0x00, 0x00, 0x48, 0x89,
	   0x97, 0x18, 0x03, 0x00, 0x00, 0x48, 0x0f, 0xb2, 0x97, 0x10, 0x03, 0x00,
	   0x00, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x4e, 0x66, 0xba, 0xf8,
	   0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74,
	   0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0x8b, 0xa7, 0x80, 0x00,
	   0x00, 0x00, 0x48, 0x89, 0xe5, 0x49, 0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0,
	   0x49, 0x66, 0xba, 0xf8, 0x03, 0xee, 0x66, 0x83, 0xc2, 0x05, 0xec, 0x24,
	   0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c, 0x89, 0xe0, 0x4c, 0x89, 0xea, 0xff,
	   0xb7, 0x18, 0x01, 0x00, 0x00, 0xff, 0xb7, 0x90, 0x00, 0x00, 0x00, 0x49,
	   0x89, 0xc4, 0x49, 0x89, 0xd5, 0xb0, 0x48, 0x66, 0xba, 0xf8, 0x03, 0xee,
	   0x66, 0x83, 0xc2, 0x05, 0xec, 0x24, 0x40, 0x3c, 0x00, 0x74, 0xf9, 0x4c,
	   0x89, 0xe0, 0x4c, 0x89, 0xea, 0x48, 0x8b, 0x07, 0x48, 0x8b, 0x4f, 0x08,
	   0x48, 0x8b, 0x57, 0x10, 0x48, 0x8b, 0x5f, 0x18, 0x48, 0x8b, 0x6f, 0x28,
	   0x48, 0x8b, 0x77, 0x30, 0x4c, 0x8b, 0x47, 0x40, 0x4c, 0x8b, 0x4f, 0x48,
	   0x4c, 0x8b, 0x57, 0x50, 0x4c, 0x8b, 0x5f, 0x58, 0x4c, 0x8b, 0x67, 0x60,
	   0x4c, 0x8b, 0x6f, 0x68, 0x4c, 0x8b, 0x77, 0x70, 0x4c, 0x8b, 0x7f, 0x78,
	   0x48, 0x8b, 0x7f, 0x38, 0x48, 0xcb
	   };*/

	//char switch_code[] = {0xc3};	// ret
	//0x0f, 0x0b, 	// invalid opcode


	// save guest cpu state of vmcb into shared memory area which is shared between BitVisor and SMM
	mem = svm_save_guest_register_to_shared_area (shared_area_vaddr);
	mem_long = (long*)mem;

	//Find page-map level-4 table base address in CR3
	asm_rdcr3 (&cr3);

	g_level4_table_base_address = svm_find_level4_table_base_address (current->u.svm.vi.vmcb->cr3);
	h_level4_table_base_address = svm_find_level4_table_base_address (cr3);

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: find level4 table base address\n");
#endif

	g_page_mem = (long*)svm_memory_map_page_table (g_level4_table_base_address);
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: memory map page table for level4 table\n");
#endif

	h_page_mem = (long*)svm_memory_map_page_table (h_level4_table_base_address);
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: memory map page table for h level4 table\n");
#endif

	// g_page_mem = level4_table
	i = svm_find_empty_entry (g_page_mem, h_page_mem);

	// backup guest cr3 and empty entry numebr to backup page table in revirt phase
	guest_cr3_backup = mem_long + GUEST_CR3_BACKUP;
	empty_entry_num_backup = mem_long + EMPTY_ENTRY_NUM_BACKUP;
	*guest_cr3_backup = current->u.svm.vi.vmcb->cr3;
	*empty_entry_num_backup = i;

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("[DEVIRT]guest cr3 backup : %llx\n", *guest_cr3_backup);
	printf ("[DEVIRT]empty entry num backup : %llx\n", *empty_entry_num_backup);
	printf ("[DEVIRT]g_page_mem : %llx\n", *(g_page_mem + *empty_entry_num_backup) );
	printf ("[DEVIRT]h_page_mem : %llx\n", *(h_page_mem + *empty_entry_num_backup) );
#endif

	if (i == -1 ) {
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
		printf ("DEVIRT: There is no level4 empty entry !!!!\n");
#endif
		return;
	}
	g_level4_empty_entry = g_page_mem + i;
	h_level4_empty_entry = h_page_mem + i;

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: empty entry number: %lld\n", i); 
#endif
	PDP_alloc_address = svm_alloc_table_to_entry (g_level4_empty_entry, 0x0);

	for (level3_entry_num = 0; level3_entry_num < 512; level3_entry_num++) {
		*(mem_long + PAGE_TABLE_AREA/8 + level3_entry_num) = PTE_P_BIT;
	}
	PD_alloc_address = svm_alloc_table_to_entry (mem_long + PAGE_TABLE_AREA/8, 0x1000);

	for (level2_entry_num = 0; level2_entry_num < 512; level2_entry_num++) {
		*(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + level2_entry_num) = PTE_P_BIT;
	}
	PT_alloc_address = svm_alloc_table_to_entry (mem_long + PAGE_TABLE_AREA/8 + 0x1000/8, 0x2000);
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: alloc table to entry for PDP, PD, PT\n");
#endif

	h_PDP_alloc_address = svm_alloc_table_to_entry (h_level4_empty_entry, 0x0);
#ifdef _PRINT_FOR_DEBUG_DEVIRT_

	printf ("[DEVIRT]g_page_mem : %llx\n", *(g_page_mem + *empty_entry_num_backup) );
	printf ("[DEVIRT]h_page_mem : %llx\n", *(h_page_mem + *empty_entry_num_backup) );

#endif
	// 0x200 = 0x1000 / 8
	// mem_long + PAGE_TABLE_AREA/8 + 0x400 is virtual address of 0th PTE
	for (level1_entry_num = 0; level1_entry_num < 512; level1_entry_num++) {
		*(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + level1_entry_num) = PTE_P_BIT;
	}
	*(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 ) = SHARED_AREA + STATE_SAVE_SIZE | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT;// | PTE_G_BIT;

	*(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 1) = SHARED_AREA | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT; // | PTE_G_BIT;

	*(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 2) = 0x4a003000 | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT;// | PTE_G_BIT;

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: substitute switch code start address to PTE 0 - %llx\n", \
			(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 ));
	printf ("DEVIRT: substitute shared area(0x48000000) to PTE 1 - %llx\n", \
			(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 1));
	printf ("DEVIRT: substitute vmcb to PTE 2\nvirtual address of PTE2: %llx\n", \
			mem_long + PAGE_TABLE_AREA/8 + 0x1000/8 + 0x1000/8 + 2);
#endif

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: guest vmcb is located in - %llx\n", \
			mem_long + PAGE_TABLE_AREA/8 + 0x3000/8);

	printf ("DEVIRT: memcpy guest's vmcb\n");
#endif
	// 0x1000 is vmcb size
	// i<<39 is virtual address of 0x49000000 (switch_code will be located in physical address 0x49000000)
	// i<<39 + 0x1000 is virtual address of shared area (0x48000000)
	// i<<39 + ox2000 is virtual address of guest's vmcb

	// copy switch_code to mem + STATE_SAVE_SIZE --> !!!warning!!! void pointer is added 1 not 8. So I use mem_long instead mem for consistency of code
	switch_code_size = sizeof(switch_code) / sizeof(char);
	memcpy ((void*)(mem_long + STATE_SAVE_SIZE/8), (void*)switch_code, switch_code_size);

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: memcpy switch code to %llx\n", (void*)(mem_long + STATE_SAVE_SIZE/8));
#endif

	/* Revirtualization: modify BitVisor page table to map (kernel) virtual address 0xffff880049000000 to 0x49000000
	 *      * FFFF884049000000 mapping start */
	svm_alloc_table_to_entry (h_page_mem + 0x110, 0x4000); // 0x110 = 0b100010000 <= 8800's first left 9 bit
	svm_alloc_table_to_entry (mem_long + PAGE_TABLE_AREA/8 + 0x4000/8 + 0x101, 0x5000);   
	svm_alloc_table_to_entry (mem_long + PAGE_TABLE_AREA/8 + 0x5000/8 + 0x48, 0x6000); 
	*(mem_long + PAGE_TABLE_AREA/8 + 0x6000/8 + 0x0) = 0x49000000 | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT;

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("BitVisor CR3 0x110 entry: %llx\n", *(h_page_mem + 0x110));
	printf ("PD 0x101 entry: %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x4000/8 + 0x101));
	printf ("PT 0x48 entry: %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x5000/8 + 0x48));
	printf ("0x4a006000's first addr: %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x6000/8 + 0x0));
#endif

	/*
	   kernel_va = (long*)0xffff884049000400;
	 *(kernel_va) = 0xdeaddead;
	 printf ("kernel_va (0xffff884049000000) : %llx\n", *(kernel_va));
	 printf ("kernel_va mapping success\n");
	 */

	/* FFFF880049000000 mapping end */

	//asm_jump_to_switch_code (mem_long + STATE_SAVE_SIZE/8, current->u.svm.vi.vmcb_phys, mem_long);

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: PDPT 0th entry    : %llx\n", *(mem_long + PAGE_TABLE_AREA/8));
	printf ("DEVIRT: PDT 0th entry     : %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x1000/8));
	printf ("DEVIRT: PT 0th entry      : %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x2000/8));
	printf ("DEVIRT: PT 1st entry      : %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x2008/8));
	printf ("DEVIRT: PT 2nd entry      : %llx\n", *(mem_long + PAGE_TABLE_AREA/8 + 0x2010/8));
	printf ("DEVIRT: current->u.svm.np : %d\n", current->u.svm.np);
	printf ("DEVIRT: guest's rflags    : %llx\n",current->u.svm.vi.vmcb->rflags);
	printf ("DEVIRT: v_intr_masking    : %d\n", current->u.svm.vi.vmcb->v_intr_masking);
	asm_rdrflags (&rflags);
	printf ("DEVIRT: RFLAGS            : %llx\n", rflags);
	asm_rdcr4 (&cr4);
	printf ("DEVIRT: CR4 register      : %llx\n", cr4);
	asm_rdcr0 (&cr0);
	printf ("DEVIRT: CR0 register      : %llx\n", cr0);
	asm_rdcr8 (&cr8);
	printf ("DEVIRT: CR8 register      : %llx\n", cr8);
	asm_rdrsp (&rsp);
	printf ("DEVIRT: RSP address: %llx, RSP content: %llx\n", rsp, *((ulong*)rsp));
	asm_rdcs (&cs);
	printf ("DEVIRT: CS selector       : %x\n", cs);
	printf ("DEVIRT: CR3 of VMCB       : %llx\n", current->u.svm.vi.vmcb->cr3);
	printf ("DEVIRT: CR3 of shared area: %llx\n", *(mem_long + 21));
	printf ("DEVIRT: CPL of VMCB       : %d\n", current->u.svm.vi.vmcb->cpl);
	printf ("DEVIRT: n_cr3 of VMCB     : %llx\n", current->u.svm.vi.vmcb->n_cr3);
	printf ("DEVIRT: EFER(SVME) of VMCB: %llx\n", current->u.svm.vi.vmcb->efer);	// check 12-bit	
	printf ("DEVIRT: lbr_virtualization_enable : %llx\n", current->u.svm.vi.vmcb->lbr_virtualization_enable);
	printf ("DEVIRT: tsc_offset        : %llx\n", current->u.svm.vi.vmcb->tsc_offset);
	printf ("DEVIRT: eventinj          : %llx\n", current->u.svm.vi.vmcb->eventinj);
	printf ("DEVIRT: interrupt_shadow  : %d\n", current->u.svm.vi.vmcb->interrupt_shadow);
	printf ("DEVIRT: memlong + 100     : %llx\n", mem_long + 100);
	printf ("DEVIRT: virtual addr of 100*8(rdi) : %llx\n", *(mem_long + 100));
	asm_rdss (&ss);
	printf ("DEVIRT: ss of bitvisor    : %x\n", ss);
	asm_rdes (&es);
	printf ("DEVIRT: es of bitvisor    : %x\n", es);
	asm_rdds (&ds);
	printf ("DEVIRT: ds of bitvisor    : %x\n", ds);
	asm_rdfs (&fs);
	printf ("DEVIRT: fs of bitvisor    : %x\n", fs);
	asm_rdgs (&gs);
	printf ("DEVIRT: gs of bitvisor    : %x\n", gs);
	asm_rdldtr (&ldtr);
	printf ("DEVIRT: ldtr of bitvisor    : %x\n", ldtr);
	asm_rdtr (&tr);
	printf ("DEVIRT: tr of bitvisor    : %x\n", tr);

#endif
	unmapmem (g_page_mem, 0x1000);
	unmapmem (h_page_mem, 0x1000);

	asm_jump_to_devirt_switch_code (i<<39, current->u.svm.vi.vmcb_phys, mem_long);

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf ("DEVIRT: return from asm_jump_to_switch_code_64\n");
#endif
}

	void*
svm_save_guest_register_to_shared_area (long *mem_long)
{
	short *mem_short = NULL;

	mem_short = (short*)mem_long;
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	printf("mapmem result mem: %llx\n", mem_long);
#endif

	// save vcpu registers into shared area (r)
	*(mem_long + RAX) = current->u.svm.vi.vmcb->rax;
	*(mem_long + RCX) = current->u.svm.vr.rcx;
	*(mem_long + RDX) = current->u.svm.vr.rdx;
	*(mem_long + RBX) = current->u.svm.vr.rbx;
	*(mem_long + RBP) = current->u.svm.vr.rbp;
	*(mem_long + RSI) = current->u.svm.vr.rsi;
	*(mem_long + RDI) = current->u.svm.vr.rdi;
	*(mem_long + R8) = current->u.svm.vr.r8;
	*(mem_long + R9) = current->u.svm.vr.r9;
	*(mem_long + R10) = current->u.svm.vr.r10;
	*(mem_long + R11) = current->u.svm.vr.r11;
	*(mem_long + R12) = current->u.svm.vr.r12;
	*(mem_long + R13) = current->u.svm.vr.r13;
	*(mem_long + R14) = current->u.svm.vr.r14;
	*(mem_long + R15) = current->u.svm.vr.r15;

	*(mem_long + RSP) = current->u.svm.vi.vmcb->rsp;
	*(mem_long + RFLAGS) = current->u.svm.vi.vmcb->rflags;
	*(mem_long + RIP) = current->u.svm.vi.vmcb->rip;
	*(mem_long + CR0) = current->u.svm.vi.vmcb->cr0;
	*(mem_long + CR2) = current->u.svm.vi.vmcb->cr2;
	*(mem_long + CR3) = current->u.svm.vi.vmcb->cr3;
	*(mem_long + CR4) = current->u.svm.vi.vmcb->cr4;
	*(mem_long + CR8) = current->u.svm.vi.vmcb->v_tpr;

	*(mem_long + DR6) = current->u.svm.vi.vmcb->dr6;
	*(mem_long + DR7) = current->u.svm.vi.vmcb->dr7;
	*(mem_long + STAR) = current->u.svm.vi.vmcb->star;
	*(mem_long + LSTAR) = current->u.svm.vi.vmcb->lstar;
	*(mem_long + CSTAR) = current->u.svm.vi.vmcb->cstar;
	*(mem_long + SFMASK) = current->u.svm.vi.vmcb->sfmask;
	*(mem_long + KERNEL_GS_BASE) = current->u.svm.vi.vmcb->kernel_gs_base;
	*(mem_long + SYSENTER_CS) = current->u.svm.vi.vmcb->sysenter_cs;
	*(mem_long + SYSENTER_ESP) = current->u.svm.vi.vmcb->sysenter_esp;
	*(mem_long + SYSENTER_EIP) = current->u.svm.vi.vmcb->sysenter_eip;

	*(mem_long + PAT) = current->u.svm.vi.vmcb->g_pat;
	*(mem_long + EFER) = current->u.svm.vi.vmcb->efer;

	// Segment Registers
	*(mem_long + CS_SEL) = current->u.svm.vi.vmcb->cs.sel;
	*(mem_long + CS_ATTR) = current->u.svm.vi.vmcb->cs.attr;
	*(mem_long + CS_LIMIT) = current->u.svm.vi.vmcb->cs.limit;
	*(mem_long + CS_BASE) = current->u.svm.vi.vmcb->cs.base;

	*(mem_long + DS_SEL) = current->u.svm.vi.vmcb->ds.sel;
	*(mem_long + DS_ATTR) = current->u.svm.vi.vmcb->ds.attr;
	*(mem_long + DS_LIMIT) = current->u.svm.vi.vmcb->ds.limit;
	*(mem_long + DS_BASE) = current->u.svm.vi.vmcb->ds.base;

	*(mem_long + ES_SEL) = current->u.svm.vi.vmcb->es.sel;
	*(mem_long + ES_ATTR) = current->u.svm.vi.vmcb->es.attr;
	*(mem_long + ES_LIMIT) = current->u.svm.vi.vmcb->es.limit;
	*(mem_long + ES_BASE) = current->u.svm.vi.vmcb->es.base;

	*(mem_long + FS_SEL) = current->u.svm.vi.vmcb->fs.sel;
	*(mem_long + FS_ATTR) = current->u.svm.vi.vmcb->fs.attr;
	*(mem_long + FS_LIMIT) = current->u.svm.vi.vmcb->fs.limit;
	*(mem_long + FS_BASE) = current->u.svm.vi.vmcb->fs.base;

	*(mem_long + GS_SEL) = current->u.svm.vi.vmcb->gs.sel;
	*(mem_long + GS_ATTR) = current->u.svm.vi.vmcb->gs.attr;
	*(mem_long + GS_LIMIT) = current->u.svm.vi.vmcb->gs.limit;
	*(mem_long + GS_BASE) = current->u.svm.vi.vmcb->gs.base;

	*(mem_long + SS_SEL) = current->u.svm.vi.vmcb->ss.sel;
	*(mem_long + SS_ATTR) = current->u.svm.vi.vmcb->ss.attr;
	*(mem_long + SS_LIMIT) = current->u.svm.vi.vmcb->ss.limit;
	*(mem_long + SS_BASE) = current->u.svm.vi.vmcb->ss.base;

	*(mem_short + GDTR_LIMIT) = current->u.svm.vi.vmcb->gdtr.limit;
	*(mem_long + GDTR_BASE) = current->u.svm.vi.vmcb->gdtr.base;

	*(mem_long + LDTR_SEL) = current->u.svm.vi.vmcb->ldtr.sel;
	*(mem_long + LDTR_ATTR) = current->u.svm.vi.vmcb->ldtr.attr;
	*(mem_long + LDTR_LIMIT) = current->u.svm.vi.vmcb->ldtr.limit;
	*(mem_long + LDTR_BASE) = current->u.svm.vi.vmcb->ldtr.base;

	*(mem_short + IDTR_LIMIT) = current->u.svm.vi.vmcb->idtr.limit;
	*(mem_long + IDTR_BASE) = current->u.svm.vi.vmcb->idtr.base;

	*(mem_long + TR_SEL) = current->u.svm.vi.vmcb->tr.sel;
	*(mem_long + TR_ATTR) = current->u.svm.vi.vmcb->tr.attr;
	*(mem_long + TR_LIMIT) = current->u.svm.vi.vmcb->tr.limit;
	*(mem_long + TR_BASE) = current->u.svm.vi.vmcb->tr.base;

#ifdef _PRINT_FOR_DEBUG_DEVIRT_
	if (REGISTER_VISIBLE) {
		//print_orig_registers();
		print_registers(mem_long);
	}
#endif

	return (void*)mem_long;
	//unmapmem (mem, len);	
}

// Make last 12 bit of level4 table base address 0 (zero)
void*
svm_find_level4_table_base_address (u64 cr3_register)
{
	ulong rsp;
	u64	level4_table_base_address = 0;

	cr3_register >>= 12;		// 51 - 12 bits are level4 table base address
	//if (DEBUG) printf ("DEVIRT: guest cr3 register: %llx\n", cr3_register);
	level4_table_base_address = cr3_register << 12;		// 52-bit is used
	//if (DEBUG) printf ("DEVIRT: level4 table base address: %llx\n", level4_table_base_address);

	return (void*)level4_table_base_address;
}

	void*
svm_memory_map_page_table (void* page_table_base_address)
{
	void *page_mem = NULL;

	page_mem = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, page_table_base_address, 0x1000);
	// 0x1000 = 4KB
	ASSERT (page_mem);

	return page_mem;
}

// It returns virtual address of start_address
long*
svm_memory_map_in_vmm (u64 start_address, u64 len)
{
	long *mem_long = NULL;
	mem_long = (long*)mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, start_address, len);
	ASSERT (mem_long);
	return mem_long;
}

	int
svm_find_empty_entry (long* g_page_table_base_address, long* h_page_table_base_address)
{
	int i = 0;
	bool there_is_empty_entry = false;

	for (i=0; i<512; i++)
	{
		/*printf ("lever4 table entry %08d: %08llx, %08llx, %08llx, %08llx\n", i, *(g_page_table_base_address + i), *(g_page_table_base_address + i + 1), *(g_page_table_base_address + i + 2), *(g_page_table_base_address + i + 3));
		  i+=3;
		  */
		if (*(g_page_table_base_address + i) == 0  && *(h_page_table_base_address + i) == 0 ) {
#ifdef _PRINT_FOR_DEBUG_DEVIRT_
			printf ("DEVIRT: empty entry: g: %llx, h: %llx\n", *(g_page_table_base_address + i), *(h_page_table_base_address + i));
			printf ("DEVIRT: empty entry number: %d\n", i);
#endif
			there_is_empty_entry = true;
			break;
		}
	}

	if ( there_is_empty_entry) {
		return i;
	} else {
		return -1;
	}
}

	long*
svm_alloc_table_to_entry (long* page_table_entry, u64 alloc_address)
{	
	alloc_address += 0x4a000000;
	*page_table_entry = alloc_address | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT;
	return (long*)alloc_address;
}

	void
print_svm_exit_code (void)
{
	switch (current->u.svm.vi.vmcb->exitcode) {
		case VMEXIT_EXCP14: /* Page fault */
			printf ("VMEXIT_EXCP14: do_pagefault ();\n");
			break;
		case VMEXIT_CR0_READ:
		case VMEXIT_CR0_WRITE:
		case VMEXIT_CR3_READ:
		case VMEXIT_CR3_WRITE:
		case VMEXIT_CR4_READ:
		case VMEXIT_CR4_WRITE:
			printf ("VMEXIT_CR0,3,4 READ and WRITE: do_readwrite_cr ();\n");
			break;
		case VMEXIT_IOIO:
			printf ("VMEXIT_IOIO: svm_ioio ();\n");
			break;
		case VMEXIT_INVLPG:
			printf ("VMEXIT_INVLPG: do_invlpg ();\n");
			break;
		case VMEXIT_TASK_SWITCH:
			printf ("VMEXIT_TASK_SWITCH: svm_task_switch ();\n");
			break;
		case VMEXIT_INTR:
			printf ("VMEXIT_INTR: do_exint_pass ();\n");
			break;
		case VMEXIT_MSR:
			printf ("VMEXIT_MSR: do_readwrite_msr ();\n");
			break;
		case VMEXIT_NPF:
			printf ("VMEXIT_NPF: do_npf ();\n");
			break;
		case VMEXIT_VMMCALL:
			printf ("VMEXIT_VMMCALL: do_vmmcall ();\n");
			break;
		case VMEXIT_INIT:
			printf ("VMEXIT_INIT: do_init ();\n");
			break;
		case VMEXIT_NMI:
			break;
		case VMEXIT_CPUID:
			printf ("VMEXIT_CPUID and VMEXIT_NMI: do_cpuid ();\n");
			break;
		default:
			printf ("unsupported exitcode\n");
	}
}

void set_state_save_area_va(u64 *state_save_area) {
	current->u.svm.state_save_area_va = state_save_area;
}

	void
svm_devirtualize ()
{	
	long *shared_area = NULL;	// for de/revirt.
	long *state_save_area = NULL;
	u64 rip = 0;			// for revirt.

	u64 r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax, rflags, cr0, cr3, cr4, efer, gdtr_base, gdtr_limit, idtr_base, idtr_limit, dr6, dr7;
	u16	cs, ds, es, ss; 	// selectors
	long *hvmcb_vaddr = NULL;
	long *gvmcb_vaddr = NULL;
	short *gvmcb_vaddr_short = NULL;
	long *gvmcb = NULL;
	short *gvmcb_short = NULL;
	long *gvmcb_for_tpr = NULL;
	struct vmcb* hvmcb;
	long *guest_cr3_backup = NULL;
	long *empty_entry_num_backup = NULL;
	int empty_entry_num = 0;
	long *g_page_mem = NULL, *h_page_mem = NULL;
	u64 star_msr_value, lstar_msr_value, cstar_msr_value, sfmask_msr_value, kernel_gs_base_msr_value, sysenter_cs_msr_value, sysenter_esp_msr_value, sysenter_eip_msr_value;
	void *g_level4_table_base_address = NULL;
	void *h_level4_table_base_address = NULL;
	//long *smm_argument = NULL;

	if (svm_devirtualizable ()) {
		printf ("devirtualizable\n");
		before_devirt = true;
		// map SHARED_AREA.
		shared_area = svm_map_shared_area();
		shared_area_addr = (u64)shared_area;

		// save host state related to VMRUN to HVMCB
		hvmcb_vaddr = shared_area + ((u64)SHARED_AREA_OFFSET_HVMCB)/sizeof(long*);
		hvmcb = (struct vmcb*)hvmcb_vaddr;
		memcpy ((void*)hvmcb, (void*)(currentcpu->svm.vmcbhost), 0x1000);	//0x1000 = PAGESIZE
		//Save hVMCB's contents using VMSAVE.
		//smm_argument = shared_area + SMM_ARGUMENT_OFFSET/8;
		//*(smm_argument) = 0;	// It becomes CMD_RESTART (2) after SMI of external machine

		// assembly start

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

		//asm_rddr7(&dr7);
		//asm_rddr6(&dr6);

		asm_rdrax(&rax);
		asm_rdrflags(&rflags);

		asm_rdcr0(&cr0);
		asm_rdcr3(&cr3);
		asm_rdcr4(&cr4);
		asm_rdmsr64 (MSR_IA32_EFER, &efer);

		asm_rdcs(&cs);
		asm_rdds(&ds);
		asm_rdes(&es);
		asm_rdss(&ss);

		asm_rdgdtr(&gdtr_base, &gdtr_limit);
		asm_rdidtr(&idtr_base, &idtr_limit);

		asm_vmsave((u64)SHARED_AREA_HVMCB);

		asm_rdmsr64(STAR_MSR, &star_msr_value);
		asm_rdmsr64(LSTAR_MSR, &lstar_msr_value);
		asm_rdmsr64(CSTAR_MSR, &cstar_msr_value);
		asm_rdmsr64(SFMASK_MSR, &sfmask_msr_value);
		asm_rdmsr64(KERNELGSBASE_MSR, &kernel_gs_base_msr_value);
		asm_rdmsr64(SYSENTER_CS_MSR, &sysenter_cs_msr_value);
		asm_rdmsr64(SYSENTER_ESP_MSR, &sysenter_esp_msr_value);
		asm_rdmsr64(SYSENTER_EIP_MSR, &sysenter_eip_msr_value);

		asm_rdrip(&rip);
#ifdef _PRINT_FOR_DEBUG_THREAD_
		printf ("rip: %x\n", rip);
#endif

		// assembly end //

		if (before_devirt == true) {
#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("memcpy hvmcb\n");
#endif

			// Save to memory.
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R15, r15);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R14, r14);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R13, r13);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R12, r12);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R11, r11);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R10, r10);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R9, r9);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_R8, r8);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RDI, rdi);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RSI, rsi);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RBP, rbp);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RSP, rsp);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RBX, rbx);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RDX, rdx);
			save_u64(shared_area, SHARED_AREA_OFFSET_MISC_RCX, rcx);

			// Save to hVMCB.	
			hvmcb->rax = rax;
			hvmcb->rsp = rsp;
			hvmcb->rip = rip;
			hvmcb->rflags = rflags;

			hvmcb->cr0 = cr0;
			hvmcb->cr3 = cr3;
			hvmcb->cr4 = cr4;
			hvmcb->efer = efer;

			/*hvmcb->dr7 = dr7;	// It died
			  hvmcb->dr6 = dr6;	// It died
#ifdef _PRINT_FOR_DEBUG_THREAD_
printf ("hvmcb->dr7: %llx\n", hvmcb->dr7);
printf ("hvmcb->dr6: %llx\n", hvmcb->dr6);
#endif*/

			hvmcb->cs.sel = cs;
			hvmcb->ds.sel = ds;
			hvmcb->es.sel = es;
			hvmcb->ss.sel = ss;

			hvmcb->gdtr.base = gdtr_base;
			hvmcb->gdtr.limit = gdtr_limit;
			hvmcb->idtr.base = idtr_base;
			hvmcb->idtr.limit = idtr_limit;

			// below states are same as vmsave result. You may be able to erase below 8 code line
			hvmcb->star = star_msr_value;
			hvmcb->lstar = lstar_msr_value; 
			hvmcb->cstar = cstar_msr_value; 
			hvmcb->sfmask = sfmask_msr_value; 
			hvmcb->kernel_gs_base = kernel_gs_base_msr_value; 
			hvmcb->sysenter_cs = sysenter_cs_msr_value; 
			hvmcb->sysenter_esp = sysenter_esp_msr_value;
			hvmcb->sysenter_eip = sysenter_eip_msr_value;

			save_gvmcb_pa(shared_area);

			/*#ifdef _PRINT_FOR_DEBUG_THREAD_
			  current->u.svm.exitcode_vmrun_bit = true;
			  printf ("Before devirt function call\n");
#endif*/
			//print_vmcb (current->u.svm.vi.vmcb);

#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("u.svm.vr.r15's value: %llx\n", current->u.svm.vr.r15 );
			printf ("u.svm.vr.r14's value: %llx\n", current->u.svm.vr.r14 );
			printf ("u.svm.vr.r13's value: %llx\n", current->u.svm.vr.r13 );
			printf ("u.svm.vr.r12's value: %llx\n", current->u.svm.vr.r12 );
			printf ("u.svm.vr.r11's value: %llx\n", current->u.svm.vr.r11 );
			printf ("u.svm.vr.r10's value: %llx\n", current->u.svm.vr.r10 );
			printf ("u.svm.vr.r9's  value: %llx\n", current->u.svm.vr.r9  );
			printf ("u.svm.vr.r8's  value: %llx\n", current->u.svm.vr.r8  );
			printf ("u.svm.vr.rdi's value: %llx\n", current->u.svm.vr.rdi );
			printf ("u.svm.vr.rsi's value: %llx\n", current->u.svm.vr.rsi );
			printf ("u.svm.vr.rbp's value: %llx\n", current->u.svm.vr.rbp );
			printf ("u.svm.vr.rbx's value: %llx\n", current->u.svm.vr.rbx );
			printf ("u.svm.vr.rdx's value: %llx\n", current->u.svm.vr.rdx );
			printf ("u.svm.vr.rcx's value: %llx\n", current->u.svm.vr.rcx );
#endif
			before_devirt = false;

			print_orig_registers();
			svm_devirt (shared_area);
		} else {
			shared_area = shared_area_addr;
			// back up guest page table and BitVisor page table
			//Find page-map level-4 table base address in CR3
			asm_rdcr3 (&cr3);

			guest_cr3_backup = shared_area + GUEST_CR3_BACKUP;
			empty_entry_num_backup = shared_area + EMPTY_ENTRY_NUM_BACKUP;

			g_level4_table_base_address = svm_find_level4_table_base_address (*guest_cr3_backup);
			h_level4_table_base_address = svm_find_level4_table_base_address (cr3);
#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("REVIRT: find level4 table base address\n");
#endif

			g_page_mem = (long*)svm_memory_map_page_table (g_level4_table_base_address);
#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("REVIRT: memory map page table for level4 table\n");
#endif

			h_page_mem = (long*)svm_memory_map_page_table (h_level4_table_base_address);
#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("REVIRT: memory map page table for h level4 table\n");
#endif

			empty_entry_num = *empty_entry_num_backup;

#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("[REVIRT]guest cr3 backup : %llx\n", *guest_cr3_backup);
			printf ("[REVIRT]empty entry num backup : %llx\n", *empty_entry_num_backup);
			printf ("[REVIRT]g_page_mem : %llx\n", *(g_page_mem + empty_entry_num) );
			printf ("[REVIRT]h_page_mem : %llx\n", *(h_page_mem + empty_entry_num) );
#endif

			*(g_page_mem + empty_entry_num) = 0;
			*(h_page_mem + empty_entry_num) = 0;

#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("[REVIRT]g_page_mem after zero : %llx\n", *(g_page_mem + empty_entry_num) );
			printf ("[REVIRT]h_page_mem after zero : %llx\n", *(h_page_mem + empty_entry_num) );
#endif

			unmapmem (g_page_mem, 0x1000);
			unmapmem (h_page_mem, 0x1000);

#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("after revirtualization\n");
#endif
			// print_vmcb (current->u.svm.vi.vmcb);
			state_save_area = shared_area + STATE_SAVE_AREA_OFFSET/8;
#ifdef _PRINT_FOR_DEBUG_THREAD_
			printf ("smm state save area: %llx\n", state_save_area);
#endif
			// guest_vmcb state save
			gvmcb_vaddr = shared_area + GVMCB_STATE_SAVE_AREA_OFFSET;	
			gvmcb_vaddr_short = (short*)gvmcb_vaddr;
			gvmcb = (long*)current->u.svm.vi.vmcb + STATE_SAVE_AREA_VMCB;
			gvmcb_short = (short*)gvmcb;
			//gvmcb_for_tpr = (long*)current->u.svm.vi.vmcb;
			*(state_save_area + 0xf) = 0x0f;
			//memcpy((void*)(gvmcb_for_tpr + 0xc), (void*)(state_save_area + 0xf), 1);

			current->u.svm.vi.vmcb->cpl = 0;
			current->u.svm.vi.vmcb->rip = *(gvmcb_vaddr + RIP_GVMCB);
			current->u.svm.vi.vmcb->efer = *(gvmcb_vaddr + EFER_GVMCB);
			current->u.svm.vi.vmcb->cr4 = *(gvmcb_vaddr + CR4_GVMCB);
			current->u.svm.vi.vmcb->cr3 = *(gvmcb_vaddr + CR3_GVMCB);
			current->u.svm.vi.vmcb->cr2 = *(gvmcb_vaddr + CR2_GVMCB);
			current->u.svm.vi.vmcb->cr0 = *(gvmcb_vaddr + CR0_GVMCB);
			current->u.svm.vi.vmcb->rflags = *(gvmcb_vaddr + RFLAGS_GVMCB);
			current->u.svm.vi.vmcb->rsp = *(gvmcb_vaddr + RSP_GVMCB);
			current->u.svm.vi.vmcb->rax = *(gvmcb_vaddr + RAX);
			printf ("LDTR_SEL_GVMCB: %x\n", *(gvmcb_vaddr_short + LDTR_SEL_GVMCB));
			current->u.svm.vi.vmcb->dr7 = *(gvmcb_vaddr + DR7_GVMCB);
			current->u.svm.vi.vmcb->dr6 = *(gvmcb_vaddr + DR6_GVMCB);
			current->u.svm.vi.vmcb->star = *(gvmcb_vaddr + STAR_GVMCB);
			current->u.svm.vi.vmcb->lstar = *(gvmcb_vaddr + LSTAR_GVMCB);
			current->u.svm.vi.vmcb->cstar = *(gvmcb_vaddr + CSTAR_GVMCB);
			current->u.svm.vi.vmcb->sfmask = *(gvmcb_vaddr + SFMASK_GVMCB);
			current->u.svm.vi.vmcb->kernel_gs_base = *(gvmcb_vaddr + KERNELGSBASE_GVMCB);
			current->u.svm.vi.vmcb->sysenter_cs = *(gvmcb_vaddr + SYSENTER_CS_GVMCB);
			current->u.svm.vi.vmcb->sysenter_esp = *(gvmcb_vaddr + SYSENTER_ESP_GVMCB);
			current->u.svm.vi.vmcb->sysenter_eip = *(gvmcb_vaddr + SYSENTER_EIP_GVMCB);
			// Consider other state-save-area values. e.g., g_pat. read AMD manual
			current->u.svm.vi.vmcb->dbgctl = *(gvmcb_vaddr + DEBUG_CTL_MSR_GVMCB);
			current->u.svm.vi.vmcb->br_from = *(gvmcb_vaddr + LAST_BRANCH_FROM_IP_MSR_GVMCB);
			current->u.svm.vi.vmcb->br_to = *(gvmcb_vaddr + LAST_BRANCH_TO_IP_MSR_GVMCB);
			current->u.svm.vi.vmcb->lastexcpfrom = *(gvmcb_vaddr + LAST_INT_FROM_IP_MSR_GVMCB);
			current->u.svm.vi.vmcb->lastexcpto = *(gvmcb_vaddr + LAST_INT_TO_IP_MSR_GVMCB);

			memcpy ((void*)(gvmcb_short + VMCB_GDTR_LIMIT_GVMCB) , (void*)(gvmcb_vaddr_short + VMCB_GDTR_LIMIT_GVMCB), 2);
			memcpy ((void*)(gvmcb + GDTR_BASE_GVMCB) , (void*)(gvmcb_vaddr + GDTR_BASE_GVMCB), 8);
			memcpy ((void*)(gvmcb_short + VMCB_IDTR_LIMIT_GVMCB) , (void*)(gvmcb_vaddr_short + VMCB_IDTR_LIMIT_GVMCB), 2);
			memcpy ((void*)(gvmcb + IDTR_BASE_GVMCB) , (void*)(gvmcb_vaddr + IDTR_BASE_GVMCB), 8);
			memcpy ((void*)(gvmcb_short + LDTR_SEL_GVMCB) , (void*)(gvmcb_vaddr_short + LDTR_SEL_GVMCB), 2);
			memcpy ((void*)(gvmcb_short + TR_SEL_GVMCB) , (void*)(gvmcb_vaddr_short + TR_SEL_GVMCB), 2);
			memcpy ((void*)(gvmcb_short + CS_ATTR_GVMCB) , (void*)(gvmcb_vaddr_short + CS_ATTR_GVMCB), 2);
			memcpy ((void*)(gvmcb + FS_BASE_CONTENTS_GVMCB) , (void*)(gvmcb_vaddr + FS_BASE_CONTENTS_GVMCB), 8);
			memcpy ((void*)(gvmcb + GS_BASE_CONTENTS_GVMCB) , (void*)(gvmcb_vaddr + GS_BASE_CONTENTS_GVMCB), 8);
			current->u.svm.vr.r15 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R15);
			current->u.svm.vr.r14 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R14);
			current->u.svm.vr.r13 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R13);
			current->u.svm.vr.r12 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R12);
			current->u.svm.vr.r11 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R11);
			current->u.svm.vr.r10 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R10);
			current->u.svm.vr.r9 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R9);
			current->u.svm.vr.r8 = *(state_save_area + STATE_SAVE_AREA_OFFSET_R8);
			current->u.svm.vr.rdi = *(state_save_area + STATE_SAVE_AREA_OFFSET_RDI);
			current->u.svm.vr.rsi = *(state_save_area + STATE_SAVE_AREA_OFFSET_RSI);
			current->u.svm.vr.rbp = *(state_save_area + STATE_SAVE_AREA_OFFSET_RBP);
			current->u.svm.vr.rbx = *(state_save_area + STATE_SAVE_AREA_OFFSET_RBX);
			current->u.svm.vr.rdx = *(state_save_area + STATE_SAVE_AREA_OFFSET_RDX);
			current->u.svm.vr.rcx = *(state_save_area + STATE_SAVE_AREA_OFFSET_RCX);

			print_orig_registers();

			set_state_save_area_va(state_save_area);
			current->u.svm.after_revirt = true;
			devirt_grant = false;
		}
	}
}

bool
svm_devirtualizable ()
{
	//printf ("in svm_devirtualizable\n");
	// devirt_grant, CPL, IF bit check
	if (devirt_grant == true && current->u.svm.vi.vmcb->cpl == 0 && ((current->u.svm.vi.vmcb->rflags & 0x1<<9) == 0 )) {
		printf ("cpl rflags devirt grant satisfied\n");
		return true;
	}
	else {
		return false;
	}
}
