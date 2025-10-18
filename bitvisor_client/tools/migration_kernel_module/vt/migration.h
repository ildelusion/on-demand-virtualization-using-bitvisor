#ifndef _MIGRATION_KERNEL_MODULE_H
#define _MIGRATION_KERNEL_MODULE_H

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */

static inline void clear_if_bit(void)
{
	asm volatile("cli");
}

static inline void set_if_bit(void)
{
	asm volatile("sti");
}

#endif
