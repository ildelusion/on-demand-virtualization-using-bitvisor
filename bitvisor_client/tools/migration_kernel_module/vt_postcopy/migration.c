#include "migration.h"
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_EMERG */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/delay.h>	/* Needed for msleep */

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
	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "postcopy_migration";

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
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (migration_vmmcall_number),	// vmmcall_number
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");
		
	set_if_bit();
	printk(KERN_EMERG "migration kernel module done. wait 100ms\n");
	msleep(100);
	printk(KERN_EMERG "migration kernel module done. wait 1s\n");
	msleep(1000);
	printk(KERN_EMERG "migration kernel module done. wait 5s\n");
	msleep(5000);
	printk(KERN_EMERG "migration kernel module done. wait 10s\n");
	msleep(10000);
	printk(KERN_EMERG "migration kernel module done. wait 20s\n");
	msleep(20000);
	printk(KERN_EMERG "migration kernel module done. wait 50s\n");
	msleep(50000);
	printk(KERN_EMERG "migration kernel module done. wait 100s\n");
	msleep(100000);
	printk(KERN_EMERG "migration kernel module done. wait 200s\n");
	msleep(200000);
	printk(KERN_EMERG "migration kernel module done. good\n");

	return 0;
}

static void __exit migration_exit(void)
{
	printk(KERN_EMERG "Exit VT migration module.\n");
}

module_init(migration_init);
module_exit(migration_exit);
