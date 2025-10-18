/*
 * This revirt.h file is appended in 2015-10-15
 * Author: Jaeseong Im, Jongyul Kim of KAIST
 * This source code is for revirtualizing BitVisor
 */
#ifndef _CORE_SVM_REVIRT_H_
#define _CORE_SVM_REVIRT_H_

#include "printf.h"
#include "pcpu.h"
#include "mm.h"
#include "current.h"
#include "assert.h"
#include "asm.h"
#include "svm_vmcb.h"
#include "constants.h"
#include "string.h"
#include "svm_devirt.h"

#define SHARED_AREA_HVMCB 					0x49001000
#define SHARED_AREA_OFFSET_HVMCB 			SHARED_AREA_HVMCB - SHARED_AREA

#define SHARED_AREA_MISC					0x49000a00
#define SHARED_AREA_OFFSET_MISC				SHARED_AREA_MISC - SHARED_AREA 
#define SHARED_AREA_OFFSET_MISC_GVMCB_PA	SHARED_AREA_OFFSET_MISC + 0x10
#define SHARED_AREA_OFFSET_MISC_R15 		SHARED_AREA_OFFSET_MISC + 0x18
#define SHARED_AREA_OFFSET_MISC_R14 		SHARED_AREA_OFFSET_MISC + 0x20
#define SHARED_AREA_OFFSET_MISC_R13 		SHARED_AREA_OFFSET_MISC + 0x28
#define SHARED_AREA_OFFSET_MISC_R12 		SHARED_AREA_OFFSET_MISC + 0x30
#define SHARED_AREA_OFFSET_MISC_R11 		SHARED_AREA_OFFSET_MISC + 0x38
#define SHARED_AREA_OFFSET_MISC_R10 		SHARED_AREA_OFFSET_MISC + 0x40
#define SHARED_AREA_OFFSET_MISC_R9 			SHARED_AREA_OFFSET_MISC + 0x48
#define SHARED_AREA_OFFSET_MISC_R8 			SHARED_AREA_OFFSET_MISC + 0x50
#define SHARED_AREA_OFFSET_MISC_RDI 		SHARED_AREA_OFFSET_MISC + 0x58
#define SHARED_AREA_OFFSET_MISC_RSI 		SHARED_AREA_OFFSET_MISC + 0x60
#define SHARED_AREA_OFFSET_MISC_RBP 		SHARED_AREA_OFFSET_MISC + 0x68
#define SHARED_AREA_OFFSET_MISC_RSP	 		SHARED_AREA_OFFSET_MISC + 0x70
#define SHARED_AREA_OFFSET_MISC_RBX 		SHARED_AREA_OFFSET_MISC + 0x78
#define SHARED_AREA_OFFSET_MISC_RDX 		SHARED_AREA_OFFSET_MISC + 0x80
#define SHARED_AREA_OFFSET_MISC_RCX 		SHARED_AREA_OFFSET_MISC + 0x88

#define STATE_SAVE_AREA_OFFSET 0x1000800

#define STATE_SAVE_AREA_OFFSET_R15	0x0	// 0x30 * 8 = 0x180
#define STATE_SAVE_AREA_OFFSET_R14	0x1
#define STATE_SAVE_AREA_OFFSET_R13	0x2
#define STATE_SAVE_AREA_OFFSET_R12	0x3
#define STATE_SAVE_AREA_OFFSET_R11	0x4
#define STATE_SAVE_AREA_OFFSET_R10	0x5
#define STATE_SAVE_AREA_OFFSET_R9	0x6
#define STATE_SAVE_AREA_OFFSET_R8	0x7
#define STATE_SAVE_AREA_OFFSET_RDI	0x8
#define STATE_SAVE_AREA_OFFSET_RSI	0x9
#define STATE_SAVE_AREA_OFFSET_RBP	0xa
#define STATE_SAVE_AREA_OFFSET_RBX	0xb
#define STATE_SAVE_AREA_OFFSET_RDX	0xc
#define STATE_SAVE_AREA_OFFSET_RCX	0xd

#define STAR_MSR			0xc0000081
#define LSTAR_MSR			0xc0000082
#define CSTAR_MSR			0xc0000083
#define SFMASK_MSR			0xc0000084
#define KERNELGSBASE_MSR	0xc0000102
#define SYSENTER_CS_MSR		0x0174
#define SYSENTER_ESP_MSR	0x0175
#define SYSENTER_EIP_MSR	0x0176

#define GVMCB_STATE_SAVE_AREA_OFFSET	0x200480	// *8 = 0x1002400 ==> 0x49002400
#define STATE_SAVE_AREA_VMCB	0x80

#define ES_GVMCB        0x0
#define CS_ATTR_GVMCB   9
#define DS_selector_GVMCB   0x18    // 24 * 2 = 48 = 0x30
#define FS_BASE_CONTENTS_GVMCB 0x9
#define GS_BASE_CONTENTS_GVMCB 0xb 
#define GDTR_BASE_GVMCB 0xd 
#define LDTR_SEL_GVMCB  0x38   
#define IDTR_BASE_GVMCB 0x11    
#define TR_SEL_GVMCB        0x48 
#define EFER_GVMCB  0x1a    //0xd0
#define CR4_GVMCB       0x29    //0x148
#define CR3_GVMCB       0x2a    //0x150
#define CR2_GVMCB       0x48    
#define CR0_GVMCB       0x2b    //0x158
#define DR7_GVMCB       0x2c    //0x160
#define DR6_GVMCB       0x2d    //0x168
#define RFLAGS_GVMCB    0x2e    //0x170
#define RIP_GVMCB       0x2f    //0x178
#define RSP_GVMCB       0x3b    //0x1d8
#define RAX_GVMCB       0x3f    //0x1f8

#define STAR_GVMCB      0x40
#define LSTAR_GVMCB     0x41
#define CSTAR_GVMCB     0x42
#define SFMASK_GVMCB            0x43
#define KERNELGSBASE_GVMCB  0x44
#define SYSENTER_CS_GVMCB       0x45
#define SYSENTER_ESP_GVMCB  0x46
#define SYSENTER_EIP_GVMCB  0x47

#define DEBUG_CTL_MSR_GVMCB           0x4e
#define LAST_BRANCH_FROM_IP_MSR_GVMCB 0x4f
#define LAST_BRANCH_TO_IP_MSR_GVMCB   0x50
#define LAST_INT_FROM_IP_MSR_GVMCB    0x51
#define LAST_INT_TO_IP_MSR_GVMCB      0x52

#define VMCB_GDTR_LIMIT_GVMCB   50  // 0x64h = 50 * 2
#define VMCB_IDTR_LIMIT_GVMCB   66  // 0x84h = 66 * 2 
//#define _PRINT_FOR_REVIT_

void print_vmcb(long *vmcb_vaddr);
void save_u64 (long *shared_area_vaddr, u64 shared_area_offset, u64 value);
void save_gvmcb_pa(long *shared_area_vaddr);

#endif	// _CORE_SVM_REVIRT_H_
