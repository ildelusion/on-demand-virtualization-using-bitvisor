#include "printf.h"
#include "vmmcall.h"
#include "initfunc.h"
#include "time.h"
#include "sleep.h"
#include <core/mmio.h>

	void
test_vmmcall()
{
	u64 t_test_invtsc[1000];
	u64 t_test_rdtsc[1000];
	u64 t_test[1000];

	printf("test_vmmcall start\n");

	print_guest_addr = 1;

	get_acpi_time (&t_test[0]);
	usleep(1000000);
	get_acpi_time (&t_test[1]);

	printf ("one second sleep using acpi: %ld\n", t_test[1] - t_test[0]);

	t_test[0] = get_cpu_time ();
	usleep(1000000);
	t_test[1] = get_cpu_time ();

	printf ("one second sleep using cpu: %ld\n", t_test[1] - t_test[0]);

	for (int i = 0; i < 1000; i++)
		get_acpi_time (&t_test_invtsc[i]);

	for (int i = 0; i < 1000; i++)
		t_test_rdtsc[i] = get_cpu_time ();

	printf ("invariant tsc: %ld\n", t_test_invtsc[999] - t_test_invtsc[0]);
	printf ("invariant tsc / 1000: %ld\n", (t_test_invtsc[999] - t_test_invtsc[0])/1000);

	printf ("rdtsc: %ld\n", t_test_rdtsc[999] - t_test_rdtsc[0]);
	printf ("rdtsc / 1000: %ld\n", (t_test_rdtsc[999] - t_test_rdtsc[0])/1000);

	printf("test_vmmcall done\n");

}

	static void
vmmcall_test_init (void)
{
	vmmcall_register("test", test_vmmcall);
}

INITFUNC("vmmcal0", vmmcall_test_init);
