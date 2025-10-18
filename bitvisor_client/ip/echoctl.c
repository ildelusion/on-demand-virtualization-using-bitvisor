#include <core/mm.h>
#include <core/process.h>
#include <core/initfunc.h>
#include "echo.h"
#include "tcpip.h"

enum echo_command {
	ECHO_CMD_CLIENT_CONNECT = 0,
	ECHO_CMD_CLIENT_SEND = 1,
	ECHO_CMD_SERVER_START = 2,
	ECHO_CMD_POSTCOPY_MIGRATION = 3,
	ECHO_CMD_TEST_SEND = 4,
	ECHO_CMD_PRINT_NETWORK = 5,
};

struct arg {
	int ipaddr_a[4];
	int port;
};

struct general_arg {
	ulong param;
};

static void
echoctl_echo_client_connect (void *arg)
{
	struct arg *a = arg;

	echo_client_init (a->ipaddr_a, a->port);
	free (a);
}

static void
echoctl_echo_client_send (void *arg)
{
	unsigned long* mem = NULL;
	long total_num_of_send = 0;
	struct mig_mem_info *mmi;

	// Null checking for the first time.
	if (arg == NULL){
		printf("(%s) arg is NULL.\n", __func__);
		return;
	} else {
		mmi = (struct mig_mem_info *) arg;
		mem = mmi->mem;
		total_num_of_send = mmi->total_send_num;
	}

	// Call function of echo-client
	echo_client_send (mem, total_num_of_send);
	free (mmi);
}

static void
echoctl_echo_client_send_postcopy_migration (void *arg)
{
	unsigned long* mem = NULL;
	long total_num_of_send = 0;
	struct mig_mem_info *mmi;

	// Null checking for the first time.
	if (arg == NULL){
		printf("(%s) arg is NULL.\n", __func__);
		return;
	} else {
		mmi = (struct mig_mem_info *) arg;
		mem = mmi->mem;
		total_num_of_send = mmi->total_send_num;
	}

	echo_client_send_postcopy_migration (mem, total_num_of_send);
	free (mmi);
}

static void
echoctl_echo_server_start (void *arg)
{
	struct arg *a = arg;

	echo_server_init (a->port);
	free (a);
}

static void
echoctl_test_send (void *arg)
{
	struct general_arg *ga = arg;
	test_send (ga->param);

	free (ga);
}

static void
echoctl_print_network (void *arg)
{
	struct general_arg *ga = arg;
	print_network (ga->param);

	free (ga);
}

static int
echoctl_sub (unsigned long (*array)[3], int len)
{
	int ret;
	ulong cmd;
	ulong ipaddr, port;
	ulong param;
	struct arg *a;
	struct general_arg *ga;
	struct mig_mem_info *mmi;

	if (len != sizeof *array)
		return -1;
	cmd = (*array)[0];
	ipaddr = (*array)[1];
	param = (*array)[1];
	port = (*array)[2];
	switch (cmd) {
	case ECHO_CMD_CLIENT_CONNECT:
		/* Connect to echo server. */
		a = alloc (sizeof *a);
		if (a) {
			a->ipaddr_a[0] = (ipaddr >> 24) & 0xff;
			a->ipaddr_a[1] = (ipaddr >> 16) & 0xff;
			a->ipaddr_a[2] = (ipaddr >>  8) & 0xff;
			a->ipaddr_a[3] = (ipaddr >>  0) & 0xff;
			a->port = (int)port;

			tcpip_begin (echoctl_echo_client_connect, a);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	case ECHO_CMD_CLIENT_SEND:
		/* Send a message to echo server. */
		mmi = alloc (sizeof *mmi);
		printf ("ECHO CMD CLIENT SEND is called\n");
		if (mmi){
			mmi->mem = (unsigned long *) (*array)[1]; // mem
			mmi->total_send_num = (long) (*array)[2]; // total_num_of_send
		} else {
			printf ("(%s) Error: mmi is not allocated.\n", __func__);
			ret = -1;
		}
		tcpip_begin (echoctl_echo_client_send, mmi);	// one time migration
		ret = 0;
		break;
	case ECHO_CMD_POSTCOPY_MIGRATION:
		mmi = alloc (sizeof *mmi);
		//printf ("ECHO CMD POSTCOPY MIGRATION is called\n");
		if (mmi){
			mmi->mem = (unsigned long *) (*array)[1]; // mem
			mmi->total_send_num = (long) (*array)[2]; // total_num_of_send
		} else {
			printf ("(%s) Error: mmi is not allocated.\n", __func__);
			ret = -1;
		}
		tcpip_begin (echoctl_echo_client_send_postcopy_migration, mmi);	// post-copy migration
		ret = 0;
		break;
	case ECHO_CMD_SERVER_START:
		/* Start echo server. */
		a = alloc (sizeof *a);
		if (a) {
			a->port = (int)port;
			tcpip_begin (echoctl_echo_server_start, a);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	case ECHO_CMD_TEST_SEND:
		// You can use param here. I added it.
		// via tcpip_begin
		ga = alloc (sizeof *ga);
		if (ga) {
			ga->param = param;
			tcpip_begin (echoctl_test_send, ga);
			ret = 0;
		} else
			ret = -1;
		break;
	case ECHO_CMD_PRINT_NETWORK:
		// You can use param here. I added it.
		// via tcpip_begin
		ga = alloc (sizeof *ga);
		if (ga) {
			ga->param = param;
			tcpip_begin (echoctl_print_network, ga);
			ret = 0;
		} else
			ret = -1;
		break;

	default:
		ret = -1;
	}
	return ret;
}

static int
echoctl_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (bufcnt != 1)
		return -1;
	return echoctl_sub (buf[0].base, buf[0].len);
}

static void
echoctl_init_msg (void)
{
	msgregister ("echoctl", echoctl_msghandler);
}

INITFUNC ("msg0", echoctl_init_msg);
