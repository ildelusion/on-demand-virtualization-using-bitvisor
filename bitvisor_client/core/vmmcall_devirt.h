#ifndef _CORE_VMMCALL_DEVIRT_H_
#define _CORE_VMMCALL_DEVIRT_H_

#include "initfunc.h"	// INITFUNC(id, func)
#include "vmmcall.h"	// for vmmcall_register
#include "types.h"

// When DIRECT_DEVIRT option set, vmmcall directly devirtualizes system. If not, vmm's mainloop wait until guest's states become suitable for devirtualization(CPL = 0 and IF = clear).
#define _DIRECT_DEVIRT_

extern bool devirt_grant;	// TODO Does it require to be per-core data? Such as, current->devirt_grant.

void devirt_vmmcall ();

#endif
