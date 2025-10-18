#include "printf.h"
#include "vmmcall.h"
#include "initfunc.h"
#include <core/mmio.h>

	void
test_vmmcall()
{
	printf("test_vmmcall start\n");

	print_guest_addr = 1;

	printf("test_vmmcall done\n");

}

	static void
vmmcall_test_init (void)
{
	vmmcall_register("test", test_vmmcall);
}

INITFUNC("vmmcal0", vmmcall_test_init);
