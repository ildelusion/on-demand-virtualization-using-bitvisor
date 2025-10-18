#include "pt_walk.h"

void *maddr_to_virt(unsigned long maddr)
{
	void *ptr = NULL;
	//ptr = (void *)(XEN_DIRECT_VIRT_START + maddr);
	ptr = (void*)phys_to_virt (maddr);
	return ptr;
}

unsigned long gfn_to_mfn (unsigned long n_cr3, unsigned long gfn)
{
	unsigned long maddr;
	int i, j, i1, i2, i3, i4;
    l1_pgentry_t *l1e;
	l2_pgentry_t *l2e;
	l3_pgentry_t *l3e;
	l4_pgentry_t *l4e;
	unsigned long gpa = gfn << PAGE_SHIFT;

	// l4 entry
	l4e = maddr_to_virt(n_cr3);
	i4 = l4_table_offset(gpa);
    if ( l4e == NULL )
    {
		//printf("l4e NULL ",n_cr3);
		return INVALID_MFN; 
	}
	if ( !(l4e_get_flags(l4e[i4]) & _PAGE_PRESENT) )
    {
		//printf("l4e entry NULL ",i4);
		return INVALID_MFN; 
	}

	// l3 entry
	maddr = l4e_get_maddr(l4e[i4]);
	l3e = maddr_to_virt(maddr);
	i3 = l3_table_offset(gpa);
	if ( l3e == NULL )
    {
		//printf("l3e NULL ",(u64)l3e);
		return INVALID_MFN; 
	}
	if ( !(l3e_get_flags(l3e[i3]) & _PAGE_PRESENT) )
    {
		//printf("l3e entry NULL ",i3);
		return INVALID_MFN; 
	}

	// l2 entry
	maddr = l3e_get_maddr(l3e[i3]);
	l2e = maddr_to_virt(maddr);
	i2 = l2_table_offset(gpa);
	if ( l2e == NULL )
    {
		//printf("l2e NULL ",(u64)l2e);
		return INVALID_MFN; 
	}
	if ( !(l2e_get_flags(l2e[i2]) & _PAGE_PRESENT) )
    {
		//printf("l2e entry NULL ",i2);
		return INVALID_MFN; 
	}

	// l2 superpage handling
	if ( l2e_get_flags(l2e[i2]) & _PAGE_PSE )
	{
		return (l2e_get_maddr(l2e[i2]) >> PAGE_SHIFT) + l1_table_offset(gpa);
	}

	// l1 entry
	maddr = l2e_get_maddr(l2e[i2]);
	l1e = maddr_to_virt(maddr);
	i1 = l1_table_offset(gpa);
	if ( l1e == NULL )
    {
		//printf("l1e NULL ",(u64)l1e);
		return INVALID_MFN; 
	}
	if ( !(l1e_get_flags(l1e[i1]) & _PAGE_PRESENT) )
    {
		//printf("l1e entry NULL ",i1);
		return INVALID_MFN; 
	}

	return l1e_get_maddr(l1e[i1]) >> PAGE_SHIFT;
}
