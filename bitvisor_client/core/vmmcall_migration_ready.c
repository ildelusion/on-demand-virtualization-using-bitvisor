#include "process.h"
#include "printf.h"
#include "vmmcall.h"
#include "initfunc.h"
#include <core.h>

static int stop_value;

void
migration_ready_vmmcall()
{
	stop_value = 1;
	while (1) {
		if(stop_value == 0)
			break;
	}
	printf("migration_ready_vmmcall: I received cpu and memory states\n");
	//after recv cpu and memory states

	printf("migration_ready_vmmcall: register virtio_handler\n");
	core_io_register_virtio_handler();
}

static void
vmmcall_migration_ready_init (void)
{
	vmmcall_register("migration_ready", migration_ready_vmmcall);
}

static int
migration_ready_sub (unsigned long (*array)[2], int len)
{
	int ret;
	ulong cmd;
	struct arg *a;

	if(len != sizeof *array)
		return -1;
	cmd = (*array)[0];

	switch(cmd) {
		case 0:
			ret = -1;
			break;
		case 1:
			stop_value = 0;
			ret = 0;
			break;
		default:
			ret = -1;
	}
}

static int
migration_ready_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if(m != MSG_BUF)
		return -1;
	if(bufcnt != 1)
		return -1;
	return migration_ready_sub(buf[0].base, buf[0].len);
}

static void
migration_ready_init_msg (void)
{
	msgregister("migration_ready", migration_ready_msghandler);
}

INITFUNC("msg0", migration_ready_init_msg);
INITFUNC("vmmcal0", vmmcall_migration_ready_init);
