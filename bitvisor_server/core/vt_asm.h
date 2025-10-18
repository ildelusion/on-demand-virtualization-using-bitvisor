#ifndef _CORE_SVM_ASM_H
#define _CORE_SVM_ASM_H

#include "linkage.h"
#include "types.h"
#include "desc.h"
#include "time.h"
#include "vmmcall_devirt.h"

//u64 devirt_ready_start=0, devirt_start=0, devirt_end=0, revirt_ready_start=0, revirt_start=0, revirt_end=0;

asmlinkage void asm_vt_jump_to_devirt_switch_code_64 (u64 switch_code_vaddr, u64 shared_area_vaddr);

// Added by JYKIM.
static inline void
asm_rdrip(ulong *rip)
{
#ifdef __x86_64__
	asm volatile ("lea (%%rip),%0" : "=rm" (*rip));
	//asm volatile ("lea (%%rip),%%rbx");
	//asm volatile ("mov %%rbx,%0" : "=r" (*rip));

#else
	asm volatile ("lea (%%eip),%0" : "=rm" (*rip));
#endif
}

// Added by JYKIM.
static inline void
asm_rdrax (ulong *rax){
	asm volatile ("mov %%rax,%0"
		      : "=r" (*rax));
}

// Added by JYKIM.
static inline void
asm_rdrbx (ulong *rbx){
	asm volatile ("mov %%rbx,%0"
		      : "=r" (*rbx));
}

// Added by JYKIM.
static inline void
asm_rdrcx (ulong *rcx){
	asm volatile ("mov %%rcx,%0"
		      : "=r" (*rcx));
}

// Added by JYKIM.
static inline void
asm_rdrdx (ulong *rdx){
	asm volatile ("mov %%rdx,%0"
		      : "=r" (*rdx));
}

// Added by JYKIM.
static inline void
asm_rdrbp (ulong *rbp){
	asm volatile ("mov %%rbp,%0"
		      : "=r" (*rbp));
}

// Added by JYKIM.
static inline void
asm_rdrsi (ulong *rsi){
	asm volatile ("mov %%rsi,%0"
		      : "=r" (*rsi));
}

// Added by JYKIM.
static inline void
asm_rdrdi (ulong *rdi){
	asm volatile ("mov %%rdi,%0"
		      : "=r" (*rdi));
}

// Added by JYKIM.
static inline void
asm_rdr8 (ulong *r8){
	asm volatile ("mov %%r8,%0"
		      : "=r" (*r8));
}

// Added by JYKIM.
static inline void
asm_rdr9 (ulong *r9){
	asm volatile ("mov %%r9,%0"
		      : "=r" (*r9));
}

// Added by JYKIM.
static inline void
asm_rdr10 (ulong *r10){
	asm volatile ("mov %%r10,%0"
		      : "=r" (*r10));
}

// Added by JYKIM.
static inline void
asm_rdr11 (ulong *r11){
	asm volatile ("mov %%r11,%0"
		      : "=r" (*r11));
}

// Added by JYKIM.
static inline void
asm_rdr12 (ulong *r12){
	asm volatile ("mov %%r12,%0"
		      : "=r" (*r12));
}

// Added by JYKIM.
static inline void
asm_rdr13 (ulong *r13){
	asm volatile ("mov %%r13,%0"
		      : "=r" (*r13));
}

// Added by JYKIM.
static inline void
asm_rdr14 (ulong *r14){
	asm volatile ("mov %%r14,%0"
		      : "=r" (*r14));
}

// Added by JYKIM.
static inline void
asm_rdr15 (ulong *r15){
	asm volatile ("mov %%r15,%0"
		      : "=r" (*r15));
}

/**
 * Jump to Switch code.
 */
inline void asm_vt_jump_to_devirt_switch_code (u64 switch_code_vaddr, u64 shared_area_vaddr)
{
#ifdef __x86_64__
	get_acpi_time(&devirt_end);
	
	printf("devirt: %llu\n", devirt_end - devirt_start);
	asm_vt_jump_to_devirt_switch_code_64 (switch_code_vaddr, shared_area_vaddr);
#else
	printf ("error!: asm_vt_jump_to_switch_code_64, no x86_64!!\n");
#endif
}

#endif
