#include "current.h"
#include "printf.h"
#include "initfunc.h"	// INITFUNC(id, func)
#include "vmmcall.h"	// for vmmcall_register
#include "svm_devirt.h"

void get_registers_vmmcall ()
{
	print_orig_registers();
}

void vmmcall_get_registers_init (void)
{
	printf ("get_registers vmmcall is called in BitVisor\n");
	vmmcall_register ("get_registers", get_registers_vmmcall);
}

INITFUNC ("vmmcal0", vmmcall_get_registers_init);
