#include "vmmcall_devirt.h"
#include "svm_devirt.h"
#include "vt_devirt.h"
#include "pcpu.h"
#include "printf.h"

bool devirt_grant = false;	// Set when DEVIRT is requested.

void
devirt_vmmcall ()
{
	printf ("devirt vmmcall called\n");
#ifdef _DIRECT_DEVIRT_
	if (currentcpu->fullvirtualize == FULLVIRTUALIZE_SVM)	// AMD_SVM
	{
		/* Devirtualization */
		devirt_grant = true;
		svm_devirtualize();
	} else if (currentcpu->fullvirtualize == FULLVIRTUALIZE_VT) {	// INTEL_VT
		/* Devirtualization */
		devirt_grant = true;
		vt_devirtualize();
	} else {
		//panic!!!
	}
#else
	printf ("devirt grant true\n");
	devirt_grant = true;
#endif
}

void
vmmcall_devirt_init (void)
{
	vmmcall_register ("devirt", devirt_vmmcall);
}

INITFUNC ("vmmcal0", vmmcall_devirt_init);
