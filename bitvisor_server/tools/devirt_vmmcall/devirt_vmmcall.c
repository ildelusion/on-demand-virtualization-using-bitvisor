#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define VMMCALL_NAME_MAXLEN 256

#include "../common/call_vmm.h"
int main (int argc, char **argv)
{
	call_vmm_function_t f;
	call_vmm_arg_t a;
	call_vmm_ret_t r;

	char buf[VMMCALL_NAME_MAXLEN] = "devirt";
	printf ("devirt vmmcall start\n");

	CALL_VMM_GET_FUNCTION ("devirt", &f);
	if (!call_vmm_function_callable (&f)) {
		fprintf (stderr, "vmmcall \"devirt\" failed\n");
		exit (1);
	}
	a.rbx = (long)buf;

	call_vmm_call_function (&f, &a, &r);

	printf ("devirt vmmcall success\n");

	return 0;
}
