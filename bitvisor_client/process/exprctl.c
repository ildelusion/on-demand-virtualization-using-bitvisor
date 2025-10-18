#include <lib_lineinput.h>
#include <lib_printf.h>
#include <lib_stdlib.h>
#include <lib_string.h>
#include <lib_syscalls.h>

static void
usage (char *hoge)
{
	printf ("%s",
		"usage:\n"
		"  start measure\n"
		"  print vmruntime\n"
		"  print appstart\n"
		"  print appexit\n"
		"  print network\n"
		"  stop measure\n"
		"  init vmruntime\n"
		"  test send [number(KB)]\n");
}

static int
send_exprctl (int d, int cmd, int param, int param2)
{
	int ret = -1;
	unsigned long array[3];
	struct msgbuf mbuf;
	
	array[0] = cmd;
	array[1] = param;
	array[2] = param2;
	setmsgbuf (&mbuf, array, sizeof array, 0);
	ret = msgsendbuf (d, 0, &mbuf, 1);
	return ret;
}

static int
action (int d, int argc, char **argv)
{
	int r, cmd, param, param2;

	if (!strcmp (argv[1], "start")) {
		if (!strcmp (argv[2], "measure")) {
			// set parameter for starting measurement
			cmd = 0;	// EXPR_START_MEASURE
		} else {
			usage (argv[0]);
			return -1;
		}
	} else if (!strcmp (argv[1], "print")) {
		if (!strcmp (argv[2], "vmruntime")) {
			// set parameter for printing vm running time
			cmd = 1;	// EXPR_PRINT_VMRUNTIME
		} else if (!strcmp (argv[2], "appstart")) {
			cmd = 2;	// EXPR_PRINT_APPSTART
		} else if (!strcmp (argv[2], "appexit")) {
			cmd = 5;	// EXPR_PRINT_APPEXIT
		} else if (!strcmp (argv[2], "network")) {
			param = (int)strtol (argv[3], NULL, 0);
			cmd = 7;	// EXPR_PRINT_NETWORK
		} else {
			usage (argv[0]);
			return -1;
		}
	} else if (!strcmp (argv[1], "stop")) {
		if (!strcmp (argv[2], "measure")) {
			cmd = 3;	// EXPR_STOP_MEASURE
		} else {
			usage (argv[0]);
			return -1;
		}
	} else if (!strcmp (argv[1], "init")) {
		if (!strcmp (argv[2], "vmruntime")) {
			cmd = 4;	// EXPR_INIT_VMRUNTIME
		} else {
			usage (argv[0]);
			return -1;
		}
	} else if (!strcmp (argv[1], "test")) {
		if (!strcmp (argv[2], "send")) {
			// argv[3] --> param
			param = (int)strtol (argv[3], NULL, 0);
			printf("(%s) test send number: %d\n", __func__, param);
			cmd = 6;	// EXPR_TEST_SEND
		}
	} else {
		usage (argv[0]);
		return -1;
	}

	r = send_exprctl (d, cmd, param, param2);
	if (r)
		printf ("Error Code: %d\n", r);
	printf ("Done.\n");
	return 0;
}

static void
parsearg (char *buf, int *argc, char **argv, int maxargc)
{
	while (*buf == ' ')
		buf++;
	*argc = 1;
	argv[0] = "";
	while (*buf != '\0' && *argc < maxargc) {
		argv[(*argc)++] = buf;
		buf = strchr (buf, ' ');
		if (!buf)
			break;
		*buf++ = '\0';
		while (*buf == ' ')
			buf++;
	}
}

int
_start (int a1, int a2)
{
	char buf[100];
	int argc;
	int *argv[10];
	int d;

	d = msgopen ("exprctl");
	if (d < 0) {
		printf ("exprctl not found.\n");
		exitprocess (1);
	}

	for (;;) {
		printf ("exprctl> ");
		lineinput (buf, 100);
		if (!strcmp (buf, ""))
			break;
		parsearg (buf, &argc, argv, 10);
		action (d, argc, argv);
	}

	msgclose (d);
	exitprocess (0);
	return 0;
}
