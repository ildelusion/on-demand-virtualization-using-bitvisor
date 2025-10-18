#ifndef _CORE_PT_WALK_H_
#define _CORE_PT_WALK_H_

#include "mm.h"

#define CONFIG_PAGING_LEVELS	4

#define L1_PAGETABLE_ENTRIES    512
#define L2_PAGETABLE_ENTRIES    512
#define L3_PAGETABLE_ENTRIES    512
#define L4_PAGETABLE_ENTRIES    512

#define _PAGE_PRESENT  0x001ULL
#define _PAGE_RW       0x002ULL
#define _PAGE_USER     0x004ULL
#define _PAGE_PWT      0x008ULL
#define _PAGE_PCD      0x010ULL
#define _PAGE_ACCESSED 0x020ULL
#define _PAGE_DIRTY    0x040ULL
#define _PAGE_PAT      0x080ULL
#define _PAGE_PSE      0x080ULL
#define _PAGE_GLOBAL   0x100ULL

#define L1_PAGETABLE_SHIFT      12
#define L2_PAGETABLE_SHIFT      21
#define L3_PAGETABLE_SHIFT      30
#define L4_PAGETABLE_SHIFT      39

#define PAGE_SHIFT              L1_PAGETABLE_SHIFT
#define ROOT_PAGETABLE_SHIFT    L4_PAGETABLE_SHIFT

#define PADDR_BITS              44
#define PADDR_MASK              ((1ULL << PADDR_BITS)-1)

#ifndef PAGE_SIZE
#define PAGE_SIZE           (1L << PAGE_SHIFT)
#endif

#define PAGE_MASK           (~(PAGE_SIZE-1))
#define PAGE_FLAG_MASK      (~0)

#define SUPERPAGE_PAGES (1UL << 9)

#define BITS_PER_LONG 64
#define BYTES_PER_LONG 8
#define LONG_BYTEORDER 3

#define PG_shift(idx)   (BITS_PER_LONG - (idx))
#define PG_mask(x, idx) (x ## UL << PG_shift(idx))

 /* The following page types are MUTUALLY EXCLUSIVE. */
#define PGT_none          PG_mask(0, 4)  /* no special uses of this page   */
#define PGT_l1_page_table PG_mask(1, 4)  /* using as an L1 page table?     */
#define PGT_l2_page_table PG_mask(2, 4)  /* using as an L2 page table?     */
#define PGT_l3_page_table PG_mask(3, 4)  /* using as an L3 page table?     */
#define PGT_l4_page_table PG_mask(4, 4)  /* using as an L4 page table?     */
#define PGT_seg_desc_page PG_mask(5, 4)  /* using this page in a GDT/LDT?  */
#define PGT_writable_page PG_mask(7, 4)  /* has writable mappings?         */
#define PGT_shared_page   PG_mask(8, 4)  /* CoW sharable page              */
#define PGT_type_mask     PG_mask(15, 4) /* Bits 28-31 or 60-63.           */


#define __PAGE_USER \
    (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_USER)
#define __PAGE_HYPERVISOR \
    (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_ACCESSED)
#define __PAGE_HYPERVISOR_NOCACHE \
    (_PAGE_PRESENT | _PAGE_RW | _PAGE_DIRTY | _PAGE_PCD | _PAGE_ACCESSED)


#define l1e_from_pfn(pfn, flags)   \
    ((l1_pgentry_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })
#define l2e_from_pfn(pfn, flags)   \
    ((l2_pgentry_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })
#define l3e_from_pfn(pfn, flags)   \
    ((l3_pgentry_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })
#define l4e_from_pfn(pfn, flags)   \
    ((l4_pgentry_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })

/* Given a virtual address, get an entry offset into a page table. */
#define l1_table_offset(a)         \
    (((a) >> L1_PAGETABLE_SHIFT) & (L1_PAGETABLE_ENTRIES - 1))
#define l2_table_offset(a)         \
    (((a) >> L2_PAGETABLE_SHIFT) & (L2_PAGETABLE_ENTRIES - 1))
#define l3_table_offset(a)         \
    (((a) >> L3_PAGETABLE_SHIFT) & (L3_PAGETABLE_ENTRIES - 1))
#define l4_table_offset(a)         \
    (((a) >> L4_PAGETABLE_SHIFT) & (L4_PAGETABLE_ENTRIES - 1))


#define get_pte_flags(x) (((int)((x) >> 40) & ~0xFFF) | ((int)(x) & 0xFFF))
#define put_pte_flags(x) (((intpte_t)((x) & ~0xFFF) << 40) | ((x) & 0xFFF))


#define l1e_get_flags(x)           (get_pte_flags((x).l1))
#define l2e_get_flags(x)           (get_pte_flags((x).l2))
#define l3e_get_flags(x)           (get_pte_flags((x).l3))
#define l4e_get_flags(x)           (get_pte_flags((x).l4))

#define l1e_get_pfn(x)             \
    ((unsigned long)(((x).l1 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))
#define l2e_get_pfn(x)             \
    ((unsigned long)(((x).l2 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))
#define l3e_get_pfn(x)             \
    ((unsigned long)(((x).l3 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))
#define l4e_get_pfn(x)             \
    ((unsigned long)(((x).l4 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))


#define l1e_get_base(x)             \
    (((x).l1 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT)
#define l2e_get_base(x)             \
    (((x).l2 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT)
#define l3e_get_base(x)             \
    (((x).l3 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT)
#define l4e_get_base(x)             \
    (((x).l4 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT)

#define l1e_get_maddr(x)             \
    (l1e_get_base(x) << PAGE_SHIFT)
#define l2e_get_maddr(x)             \
    (l2e_get_base(x) << PAGE_SHIFT)
#define l3e_get_maddr(x)             \
    (l3e_get_base(x) << PAGE_SHIFT)
#define l4e_get_maddr(x)             \
    (l4e_get_base(x) << PAGE_SHIFT)
#define l2e_get_l1e(x)             \
	((l1_pgentry_t *)(l2e_get_base(x) << PAGE_SHIFT))
#define l3e_get_l2e(x)             \
	((l2_pgentry_t *)(l3e_get_base(x) << PAGE_SHIFT))
#define l4e_get_l3e(x)             \
	((l3_pgentry_t *)(l4e_get_base(x) << PAGE_SHIFT))

#define INVALID_MFN		(~0UL)
#define INVALID_DOMID	(~0U)
#define MFN_UNSHARED		0
#define MFN_SHARED			1


#define MFN_4G_LIMIT	(1UL << (32 - PAGE_SHIFT))
#define GFN_BIOS_START	0x0	
#define	GFN_BIOS_END	(GFN_BIOS_START + 0xff)
#define GFN_SP_START	0xf0000
#define GFN_SP_END		(GFN_SP_START + 0x10000 -1)

typedef u64 intpte_t;

typedef struct { intpte_t l1; } l1_pgentry_t;
typedef struct { intpte_t l2; } l2_pgentry_t;
typedef struct { intpte_t l3; } l3_pgentry_t;
typedef struct { intpte_t l4; } l4_pgentry_t;
typedef l4_pgentry_t root_pgentry_t;

void *maddr_to_virt(unsigned long maddr);

unsigned long gfn_to_mfn (unsigned long n_cr3, unsigned long gfn);


#endif
