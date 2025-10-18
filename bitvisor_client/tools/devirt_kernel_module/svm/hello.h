#ifndef _DEVIRT_KERNEL_MODULE_H
#define _DEVIRT_KERNEL_MODULE_H

static inline void clear_if_bit(void)
{
	/*unsigned long rflags;
	asm volatile("pushfq; \
			popq %0 ": "=rm" (rflags));
	rflags = rflags & 0xfffffffffffffdff;
	rflags = rflags | 0x0200000;
	asm volatile("pushq %0;\
			popfq" : "=rm" (rflags));*/
	asm volatile("cli");
}

static inline void set_if_bit(void)
{
	asm volatile("sti");
}
#endif
