#include "printf.h"
#include "vmmcall.h"
#include "initfunc.h"

void after_migration_vmmcall()
{
	static int i = 0;
	printf("number: %d, after_migration\n", i);
}

static int vmmcall_after_migration_init (void)
{
	vmmcall_register ("after_migration", after_migration_vmmcall);
}

INITFUNC("vmmcal0", vmmcall_after_migration_init);
