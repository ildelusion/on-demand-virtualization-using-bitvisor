#ifndef CPU_X86_SMM_H
#define CPU_X86_SMM_H

#include <smm_types.h>

typedef struct smm_segment_attribute
{
        uint16_t type:4;    /* 0;  Bit 40-43 */
        uint16_t s:   1;    /* 4;  Bit 44 */
        uint16_t dpl: 2;    /* 5;  Bit 45-46 */
        uint16_t p:   1;    /* 7;  Bit 47 */
        uint16_t avl: 1;    /* 8;  Bit 52 */
        uint16_t l:   1;    /* 9;  Bit 53 */
        uint16_t db:  1;    /* 10; Bit 54 */
        uint16_t g:   1;    /* 11; Bit 55 */
        uint16_t pad: 4;
} __attribute__ ((packed)) smm_segattr_t;

typedef struct {

	u8	reserved[0x570];
	u64	rflags;
	u64	rip;

} __attribute__((packed)) host_state_save_area_t;


typedef struct {

	u16	es_selector;
	u8	es_reserved[6];
	u64	es_discriptor;

	u16	cs_selector;
	smm_segattr_t cs_attribute;
	u32 cs_limit;
	u64	cs_base;

	u16	ss_selector;
	u8	ss_reserved[6];
	u64	ss_discriptor;

	u16	ds_selector;
	u8	ds_reserved[6];
	u64	ds_discriptor;

	u16	fs_selector;
	u8	fs_reserved[2];
	u32	fs_base;
	u64	fs_discriptor;

	u16	gs_selector;
	u8	gs_reserved[2];
	u32	gs_base;
	u64	gs_discriptor;

	u8	gdtr_reserved0[4];
	u16	gdtr_limit;
	u8	gdtr_reserved1[2];
	u64	gdtr_descriptor;

	u16	ldtr_selector;
	u16	ldtr_attributes;
	u32	ldtr_limit;
	u64	ldtr_base;

	u8	idtr_reserved0[4];
	u16	idtr_limit;
	u8	idtr_reserved1[2];
	u64	idtr_base;

	u16	tr_selector;
	u16	tr_attributes;
	u32	tr_limit;
	u64	tr_base;

	u64 iorestart_rip;
	u64 iorestart_rcx;
	u64 iorestart_rsi;
	u64 iorestart_rdi;

	u32 io_trap_offset;
	u32 local_smi_status;
	u8	iorestart_byte;
	u8	iorestart_autohalt;
	u8	nmi_mask;

	u8	reserved0[5];

	u64 efer;
	u64 svm_state;
	u64 svm_vmcb;
	u64 svm_vinterrupt;

	u8	reserved1[12];

	u32 smm_revision;
	u32 smm_baseaddr;

	u8	reserved2[28];

	u64	guest_pat;
	u64	host_efer;
	u64	host_cr4;
	u64	host_cr3;
	u64	host_cr0;

	u64	cr4;
	u64	cr3;
	u64	cr0;
	u64	dr7;
	u64	dr6;

	u64	rflags;
	u64	rip;
	u64	r15;
	u64	r14;
	u64	r13;
	u64	r12;
	u64	r11;
	u64	r10;
	u64	r9;
	u64	r8;

	u64	rdi;
	u64	rsi;
	u64	rbp;
	u64	rsp;
	u64	rbx;
	u64	rdx;
	u64	rcx;
	u64	rax;

} __attribute__((packed)) smm_state_save_area_t;


#endif
