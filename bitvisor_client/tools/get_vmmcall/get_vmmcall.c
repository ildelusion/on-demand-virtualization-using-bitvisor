#include <stdio.h>

#define VMMCALL_NAME_MAXLEN 256

typedef struct {
	unsigned long rbx, rcx, rdx, rsi, rdi;
} call_vmm_arg_t;

typedef struct {
	unsigned long rax, rbx, rcx, rdx, rsi, rdi;
} call_vmm_ret_t;

int main(void)
{
	call_vmm_arg_t a;
	call_vmm_ret_t r;

	char vmmcall_name[VMMCALL_NAME_MAXLEN] = "devirt";

	a.rbx = (long)vmmcall_name;

	asm volatile ("vmmcall"
			: "=a" (r.rax), "=b" (r.rbx),
			"=c" (r.rcx), "=d" (r.rdx),
			"=S" (r.rsi), "=D" (r.rdi)
			: "a" (0),	// vmmcall_number == 0 
			"b" (a.rbx),
			"c" (a.rcx), "d" (a.rdx),
			"S" (a.rsi), "D" (a.rdi)
			: "memory");

	printf ("vmmcall_number is %d\n", r.rax);

	return 0;

}
