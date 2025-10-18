#include "migration_module.h"
#include "assem.h"

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
	int migration_vmmcall_num = 0;
	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "migration";

	// Get vmmcall number.
	a.rbx = (long)vmmcall_name;
	a.rcx = 0;
	a.rdx = 0;
	a.rsi = 0;
	a.rdi = 0;
	
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (0),	// vmmcall_number == 0, It calls get_vmmcall vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	printk(KERN_EMERG "vmmcall_number is %ld\n", r.rax);
	migration_vmmcall_num = r.rax;


	// Clear IF bit. 	
	clear_if_bit();

	// Do migration vmmcall.
	asm volatile ("vmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (migration_vmmcall_num),	// vmmcall number is set to vmmcall_migration.
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	// Excuted after migration done.
	set_if_bit();

	printk(KERN_EMERG "Migration kernel module done.\n");
	return 0;
}

static void __exit migration_exit(void)
{
	printk(KERN_EMERG "Exit VT migration module.\n");
}

module_init(migration_init);
module_exit(migration_exit);
