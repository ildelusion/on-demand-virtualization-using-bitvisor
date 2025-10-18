#include "vmmcall_migration.h"
#include <core.h>
#include "sleep.h"
//#include "pcpu.h"
//#include "printf.h"

void
migration_vmmcall ()
{
	
}

void
vmmcall_migration_init (void)
{
	vmmcall_register ("migration", migration_vmmcall);
}

INITFUNC ("vmmcal0", vmmcall_migration_init);
