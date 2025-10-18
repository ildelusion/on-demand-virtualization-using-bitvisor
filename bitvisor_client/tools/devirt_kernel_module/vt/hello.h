#ifndef _DEVIRT_KERNEL_MODULE_H
#define _DEVIRT_KERNEL_MODULE_H

static inline void clear_if_bit(void)
{
	asm volatile("cli");
}

static inline void set_if_bit(void)
{
	asm volatile("sti");
}
#endif
