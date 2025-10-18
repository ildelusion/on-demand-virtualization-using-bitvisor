#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "cpu_emul.h"
#include "cpu_mmu.h"
#include "current.h"
#include "exint_pass.h"
#include "gmm_pass.h"
#include "initfunc.h"
#include "int.h"
#include "linkage.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "reboot.h"
#include "string.h"
#include "thread.h"
#include "vmmcall.h"
#include "vmmcall_status.h"
#include "vt.h"
#include "vt_addip.h"
#include "vt_exitreason.h"
#include "vt_init.h"
#include "vt_io.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"
#include "vt_vmcs.h"
#include "vt_devirt.h"
#include "process.h"
#include "initfunc.h"

enum expr_command {
	EXPR_START_MEASURE = 0,
	EXPR_PRINT_VMRUNTIME = 1,
	EXPR_PRINT_APPSTART = 2,
	EXPR_STOP_MEASURE = 3,
	EXPR_INIT_VMRUNTIME = 4,
	EXPR_PRINT_APPEXIT = 5,
	EXPR_TEST_SEND = 6,
	EXPR_PRINT_NETWORK = 7,
};

static void
exprctl_start_measure(void)
{
	time_measure = true;
}

static void
exprctl_stop_measure(void)
{
	time_measure = false;
}

static void
exprctl_print_vmruntime(void)
{
	for (int i = 0; i < TIME_MEASURE; i++) {
		printf ("%d\n", vmrun_time[i]);
	}
	printf ("End of vmrun time\n");
}

static void
exprctl_init_vmruntime(void)
{
	t_area = 0;
	first_new_start = true;
	for (int i = 0; i < TIME_MEASURE; i++) {
		vmrun_time[i] = 0;
	}
}

static void
exprctl_print_appstart(void)
{
	printf ("app_start time: %d\n", t_area);
}

static void
exprctl_print_appexit(void)
{
	printf ("app_exit time: %d\n", t_area);
}

static int
exprctl_test_send(int param)
{
	unsigned long array_echoctl[3];
	int d_echoctl;		//echoctl msg descriptor id.
	int ret_echoctl = -1;
	struct msgbuf mbuf_echoctl;

	printf ("%s called\n", __func__);

	d_echoctl = msgopen("echoctl");
	if(d_echoctl < 0) {
		printf("echoctl msg descriptor not found.\n");
		ret_echoctl = -1;
	}
	
	array_echoctl[0] = 4;
	array_echoctl[1] = param;
	array_echoctl[2] = 0;

	setmsgbuf (&mbuf_echoctl, array_echoctl, sizeof array_echoctl, 0);
	ret_echoctl = msgsendbuf(d_echoctl, 0, &mbuf_echoctl, 1);
	msgclose(d_echoctl);
	return ret_echoctl;
}

static void 
exprctl_print_network(int param)
{
	unsigned long array_echoctl[3];
	int d_echoctl;		//echoctl msg descriptor id.
	int ret_echoctl = -1;
	struct msgbuf mbuf_echoctl;

	d_echoctl = msgopen("echoctl");
	if(d_echoctl < 0) {
		printf("echoctl msg descriptor not found.\n");
		ret_echoctl = -1;
	}

	array_echoctl[0] = 5;
	array_echoctl[1] = param;
	array_echoctl[2] = 0;

	setmsgbuf (&mbuf_echoctl, array_echoctl, sizeof array_echoctl, 0);
	ret_echoctl = msgsendbuf(d_echoctl, 0, &mbuf_echoctl, 1);
	msgclose(d_echoctl);
	return ret_echoctl;
}

static int
exprctl_sub (unsigned long (*array)[3], int len)
{
	int ret = 0;
	ulong cmd;
	ulong param, param2;
	struct arg *a;

	if (len != sizeof *array)
		return -1;
	cmd = (*array)[0];
	param = (*array)[1];
	param2 = (*array)[2];
	switch (cmd) {
		case EXPR_START_MEASURE:
			// Do start measure
			printf ("Start measure in %s\n", __func__);
			exprctl_start_measure();
			break;
		case EXPR_PRINT_VMRUNTIME:
			// Do print vmruntime
			printf ("Print vmruntime in %s\n", __func__);
			exprctl_print_vmruntime();
			break;
		case EXPR_PRINT_APPSTART:
			printf ("Print application start time in %s\n", __func__);
			exprctl_print_appstart();
			break;
		case EXPR_STOP_MEASURE:
			printf ("Stop measure in %s\n", __func__);
			exprctl_stop_measure();
			break;
		case EXPR_INIT_VMRUNTIME:
			printf ("Initialize vmrun time in %s\n", __func__);
			exprctl_init_vmruntime();
			break;
		case EXPR_PRINT_APPEXIT:
			printf ("Print application start time in %s\n", __func__);
			exprctl_print_appexit();
			break;
		case EXPR_TEST_SEND:
			printf ("Test send number: %d in %s\n", param, __func__);
			// impl
			exprctl_test_send(param);
			break;
		case EXPR_PRINT_NETWORK:
			printf ("Print network related status in %s\n", __func__);
			exprctl_print_network(param);
			break;
	}
	return ret;
}

static int
exprctl_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (bufcnt != 1)
		return -1;
	return exprctl_sub (buf[0].base, buf[0].len);
}

static void
exprctl_init_msg (void)
{
	msgregister ("exprctl", exprctl_msghandler);
}

INITFUNC ("msg0" , exprctl_init_msg);
