/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "current.h"
#include "gmm_access.h"
#include "mm.h"
#include "panic.h"
#include "string.h"
#include "vt_ept.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"
#include "thread.h"	// schedule()
#include "process.h"	// to use msgopen, setmsgbuf, msgsendbuf, msgclose
#include "time.h"

#define NUM_OF_EPTBL	1024
#define EPTE_READ	0x1
#define EPTE_READEXEC	0x5
#define EPTE_WRITE	0x2
#define EPTE_LARGE	0x80
#define EPTE_ATTR_MASK	0xFFF
#define EPTE_MT_SHIFT	3
#define EPT_LEVELS	4

//#define NUM_OF_MEM_AREA 6
#define NUM_OF_MEM_AREA 7

struct usable_mem {
	long start_addr;
	long end_addr;
};

static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	{0x100000000, 0x4767fffff},
};

struct vt_ept {
	int cnt;
	int cleared;
	void *ncr3tbl;
	phys_t ncr3tbl_phys;
	void *tbl[NUM_OF_EPTBL];
	phys_t tbl_phys[NUM_OF_EPTBL];
	struct {
		int level;
		phys_t gphys;
		u64 *entry[EPT_LEVELS];
	} cur;
};

void
vt_ept_init (void)
{
	struct vt_ept *ept;
	int i;

	ept = alloc (sizeof *ept);
	alloc_page (&ept->ncr3tbl, &ept->ncr3tbl_phys);
	printf("ncr3tbl virtual addr: %lx\n ncr3tbl_phys addr: %lx\n", ept->ncr3tbl, ept->ncr3tbl_phys);
	memset (ept->ncr3tbl, 0, PAGESIZE);
	ept->cleared = 1;
	for (i = 0; i < NUM_OF_EPTBL; i++)
		alloc_page (&ept->tbl[i], &ept->tbl_phys[i]);
	ept->cnt = 0;
	ept->cur.level = EPT_LEVELS;
	current->u.vt.ept = ept;
	asm_vmwrite64 (VMCS_EPT_POINTER, ept->ncr3tbl_phys |
		       VMCS_EPT_POINTER_EPT_WB | VMCS_EPT_PAGEWALK_LENGTH_4);
}

static void
cur_move (struct vt_ept *ept, u64 gphys)
{
	u64 mask, *p, e;

	mask = 0xFFFFFFFFFFFFF000ULL;
	if (ept->cur.level > 0)
		mask <<= 9 * ept->cur.level;
	while (ept->cur.level < EPT_LEVELS &&
	       (gphys & mask) != (ept->cur.gphys & mask)) {
		ept->cur.level++;
		mask <<= 9;
	}
	ept->cur.gphys = gphys;
	if (!ept->cur.level)
		return;
	if (ept->cur.level >= EPT_LEVELS) {
		p = ept->ncr3tbl;
		p += (gphys >> (EPT_LEVELS * 9 + 3)) & 0x1FF;
		ept->cur.entry[EPT_LEVELS - 1] = p;
		ept->cur.level = EPT_LEVELS - 1;
	} else {
		p = ept->cur.entry[ept->cur.level];
	}
	while (ept->cur.level > 0) {
		e = *p;
		if (!(e & EPTE_READ) || (e & EPTE_LARGE))
			break;
		e &= ~PAGESIZE_MASK;
		e |= (gphys >> (9 * ept->cur.level)) & 0xFF8;
		p = (u64 *)phys_to_virt (e);
		ept->cur.level--;
		ept->cur.entry[ept->cur.level] = p;
	}
}

static u64 *
cur_fill (struct vt_ept *ept, u64 gphys, int level)
{
	int l;
	u64 *p;

	if (ept->cnt + ept->cur.level - level > NUM_OF_EPTBL) {
		memset (ept->ncr3tbl, 0, PAGESIZE);
		ept->cleared = 1;
		ept->cnt = 0;
		vt_paging_flush_guest_tlb ();
		ept->cur.level = EPT_LEVELS - 1;
	}
	l = ept->cur.level;
	for (p = ept->cur.entry[l]; l > level; l--) {
		*p = ept->tbl_phys[ept->cnt] | EPTE_READEXEC | EPTE_WRITE;
		p = ept->tbl[ept->cnt++];
		memset (p, 0, PAGESIZE);
		p += (gphys >> (9 * l + 3)) & 0x1FF;
	}
	return p;
}

static void
vt_ept_map_page_sub (struct vt_ept *ept, bool write, u64 gphys)
{
	bool fakerom;
	u64 hphys;
	u32 hattr;
	u64 *p;

	cur_move (ept, gphys);
	p = cur_fill (ept, gphys, 0);
	hphys = current->gmm.gp2hp (gphys, &fakerom) & ~PAGESIZE_MASK;
	if (fakerom && write)
		panic ("EPT: Writing to VMM memory.");
	hattr = (cache_get_gmtrr_type (gphys) << EPTE_MT_SHIFT) |
		EPTE_READEXEC | EPTE_WRITE;
	if (fakerom)
		hattr &= ~EPTE_WRITE;
	*p = hphys | hattr;
}

static bool
vt_ept_map_2mpage (struct vt_ept *ept, u64 gphys)
{
	u64 hphys;
	u32 hattr;
	u64 *p;

	cur_move (ept, gphys);
	if (!ept->cur.level)
		return true;
	hphys = current->gmm.gp2hp_2m (gphys & ~PAGESIZE2M_MASK);
	if (hphys == GMM_GP2HP_2M_FAIL)
		return true;
	if (!cache_gmtrr_type_equal (gphys & ~PAGESIZE2M_MASK,
				     PAGESIZE2M_MASK))
		return true;
	hattr = (cache_get_gmtrr_type (gphys & ~PAGESIZE2M_MASK) <<
		 EPTE_MT_SHIFT) | EPTE_READEXEC | EPTE_WRITE | EPTE_LARGE;
	p = cur_fill (ept, gphys, 1);
	*p = hphys | hattr;
	return false;
}

static int
vt_ept_level (struct vt_ept *ept, u64 gphys)
{
	cur_move (ept, gphys);
	return ept->cur.level;
}

static void
vt_ept_map_page_clear_cleared (struct vt_ept *ept)
{
	u32 n, nn;
	u64 base, len, size;
	phys_t next_phys;

	ept->cleared = 0;
	n = 0;
	for (nn = 1; nn; n = nn) {
		nn = current->gmm.getforcemap (n, &base, &len);
		if (!len)
			continue;
		len += base & PAGESIZE_MASK;
		base &= ~PAGESIZE_MASK;
		while (len > 0) {
			size = PAGESIZE;
			if (vt_ept_level (ept, base) > 0 &&
			    !mmio_range (base & ~PAGESIZE2M_MASK, PAGESIZE2M)
			    && !vt_ept_map_2mpage (ept, base))
				size = (base | PAGESIZE2M_MASK) + 1 - base;
			else if (!(next_phys = mmio_range (base, PAGESIZE)))
				vt_ept_map_page_sub (ept, true, base);
			else
				size = (next_phys - base + PAGESIZE - 1) &
					~PAGESIZE_MASK;
			if (size > len)
				size = len;
			base += size;
			len -= size;
		}
	}
	if (ept->cleared)
		panic ("%s: error", __func__);
}

static void
vt_ept_map_page (struct vt_ept *ept, bool write, u64 gphys)
{
	if (ept->cleared)
		vt_ept_map_page_clear_cleared (ept);
	vt_ept_map_page_sub (ept, write, gphys);
	if (ept->cleared)
		vt_ept_map_page_clear_cleared (ept);
}

static bool
vt_migration_range (u64 gphys)
{
	int i;
	bool in_range = false;

	for (i = 0; i < NUM_OF_MEM_AREA; i++) {
		if (gphys >= usable_mem_list[i].start_addr && gphys <= usable_mem_list[i].end_addr)
			in_range = true;
	}

	return in_range;
}

static void
send_msg_addr_req (u64 gphys)
{
	int ret_echoctl = -1;
	u64 array_echoctl[3];
	struct msgbuf mbuf_echoctl;
	int d_echoctl;

	d_echoctl = msgopen("echoctl");
	if(d_echoctl < 0) {
		printf("echoctl msg descriptor not found.\n");
		return;
	}

	array_echoctl[0] = 4;	// new msg type
	array_echoctl[1] = gphys;
	array_echoctl[2] = 0;

	setmsgbuf (&mbuf_echoctl, array_echoctl, sizeof array_echoctl, 0);

	ret_echoctl = msgsendbuf(d_echoctl, 0, &mbuf_echoctl, 1);
	msgclose (d_echoctl);
	return ret_echoctl;
}

void
vt_ept_violation (bool write, u64 gphys)
{
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	
	mmio_lock ();
	if (!mmio_access_page (gphys, true)) {
		//if (aft_mig_in_mmio == 1 && vt_migration_range (gphys)) {
		if (aft_mig_in_mmio == 1) {
			if (vt_migration_range (gphys)) {
				// sync version of vt_ept_violation START

				// measuring time get_acpi_time
				static int j = 0;
				static int i = 0;
				//printf("vt_ept_violation, requested gphys: %lx\n", gphys);
				/*get_acpi_time (&t_vm[j][1]);
				  t_exit_ept[i++] = t_vm[j][1] - t_exit;*/	// 2019-03-24
				j++;
				//ept_recv_flag = true;	// 2018-11-12 for measuring time

				send_msg_addr_req (gphys);
				while (!ept_recved)
				{
					if (aft_mig_in_mmio == 0) {
						break;
					}
					schedule();
				}
				//get_acpi_time (&t4);
				// sync version END

				//mmio_lock ();
				vt_ept_map_page (ept, write, gphys);
				ept_recved = false;
				//printf("mapped gphys: %lx\n", gphys);
			} else {
				vt_ept_map_page (ept, write, gphys);
			}
		} else {
			//mmio_lock ();
			vt_ept_map_page (ept, write, gphys);
		}
	}
	//if (current->u.vt.aft_mig == 1) { printf("do_ept_violation: VMCS_GUEST_PHYSICAL_ADDRESS: %lx\n", gp); }
	mmio_unlock ();
}

// for stop-and-copy migration
/*
void
vt_ept_violation (bool write, u64 gphys)
{
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	
	if (!mmio_access_page (gphys, true)) {
		mmio_lock ();
		vt_ept_map_page (ept, write, gphys);
	}
	mmio_unlock ();
}
*/

void
vt_ept_tlbflush (void)
{
}

void
vt_ept_updatecr3 (void)
{
	ulong cr3, cr4;
	u64 tmp64;

	vt_paging_flush_guest_tlb ();
	if (!current->u.vt.lma && current->u.vt.vr.pg) {
		asm_vmread (VMCS_CR4_READ_SHADOW, &cr4);
		if (cr4 & CR4_PAE_BIT) {
			asm_vmread (VMCS_GUEST_CR3, &cr3);
			cr3 &= 0xFFFFFFE0;
			read_gphys_q (cr3 + 0x0, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE0, tmp64);
			read_gphys_q (cr3 + 0x8, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE1, tmp64);
			read_gphys_q (cr3 + 0x10, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE2, tmp64);
			read_gphys_q (cr3 + 0x18, &tmp64, 0);
			asm_vmwrite64 (VMCS_GUEST_PDPTE3, tmp64);
		}
	}
}

void
vt_ept_clear_all (void)
{
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	memset (ept->ncr3tbl, 0, PAGESIZE);
	ept->cleared = 1;
	ept->cnt = 0;
	ept->cur.level = EPT_LEVELS;
	vt_paging_flush_guest_tlb ();
}

bool
vt_ept_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp1, tmp2, mask = p->pte_addr_mask;
	unsigned int cnt, i, j, n = 512;
	struct vt_ept *ept;

	ept = p->u.vt.ept;
	cnt = ept->cnt;
	for (i = 0; i < cnt; i++) {
		e = ept->tbl[i];
		for (j = 0; j < n; j++) {
			if (!(e[j] & EPTE_READ))
				continue;
			tmp1 = e[j] & mask;
			tmp2 = tmp1 | 07777;
			if (e[j] & EPTE_LARGE) {
				tmp1 &= ~07777777;
				tmp2 |= 07777777;
			}
			if (start <= tmp2 && tmp1 <= end) {
				if (p != current)
					return true;
				e[j] = 0;
			}
		}
	}
	return false;
}

void
vt_ept_map_1mb (void)
{
	ulong gphys;
	struct vt_ept *ept;

	ept = current->u.vt.ept;
	vt_ept_clear_all ();
	for (gphys = 0; gphys < 0x100000; gphys += PAGESIZE) {
		mmio_lock ();
		if (!mmio_access_page (gphys, false))
			vt_ept_map_page (ept, false, gphys);
		mmio_unlock ();
	}
}
