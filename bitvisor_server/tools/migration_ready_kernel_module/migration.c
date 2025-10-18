#include "migration.h"
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_EMERG */
#include <linux/init.h>		/* Needed for the macros */

#define VMMCALL_NAME_MAXLEN 256

typedef struct {
	unsigned long rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	unsigned long rax, rbx, rcx, rdx, rsi, rdi;
} call_vmm_ret_t;


static int 
__init migration_init (void)
{
	call_vmm_arg_t a;
	call_vmm_ret_t r;
	int migration_vmmcall_number = 0;
	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "migration_ready";

	a.rbx = (long)vmmcall_name;
	
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (0),	// vmmcall_number == 0, It calls get_vmmcall vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	printk(KERN_EMERG "vmmcall_number is %d\n", r.rax);
	migration_vmmcall_number = r.rax;

	clear_if_bit();

	printk(KERN_EMERG "migration_ready kernel module called\n");

	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (migration_vmmcall_number),	// vmmcall_number
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");
		
	printk(KERN_EMERG "after migration done\n");

	set_if_bit();
	printk(KERN_EMERG "migration kernel module done.\n");
	return 0;
}

static void __exit migration_exit(void)
{
	printk(KERN_EMERG "Exit VT migration module.\n");
}

module_init(migration_init);
module_exit(migration_exit);
