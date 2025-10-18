/*  
 *   *  hello-2.c - Demonstrating the module_init() and module_exit() macros.
 *   *  This is preferred over using init_module() and cleanup_module().
 *   */
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
//#include "/lib/modules/3.16.37/build/arch/x86/include/asm/special_insns.h"
#include "hello.h"

#define VMMCALL_NAME_MAXLEN 256

typedef struct {
	unsigned long rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	unsigned long rax, rbx, rcx, rdx, rsi, rdi;
} call_vmm_ret_t;

static int __init hello_init(void)
{
	//unsigned long rflags;
	call_vmm_arg_t a;
	call_vmm_ret_t r;
	int get_registers_vmmcall_number = 0;
	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "get_registers";

	a.rbx = (long)vmmcall_name;
	
	printk(KERN_INFO "get registers start\n");
	asm volatile ("vmmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (0),	// vmmcall_number == 0, It calls get_vmmcall vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	printk(KERN_INFO "vmmcall_number is %d\n", r.rax);
	get_registers_vmmcall_number = r.rax;

	clear_if_bit();

	asm volatile ("vmmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (get_registers_vmmcall_number),	// vmmcall_number. It is get_registeres vmmcall
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	set_if_bit();

	printk(KERN_INFO "get registers success\n");

	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_INFO "Goodbye, world\n");
}

module_init(hello_init);
module_exit(hello_exit);
