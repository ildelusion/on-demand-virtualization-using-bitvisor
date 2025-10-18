#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>    /* errno */
#include <sys/mman.h> /* mmap(), munmap(), PROT_READ, PROT_WRITE, MAP_SHARED */
//#include <xenctrl.h>
#include "libSMM.h"
#include <smm_types.h>
#include <sys/syscall.h> 
#include <unistd.h> 
#include <fcntl.h>	/* O_RDWR */

#include <sys/time.h>
#include <time.h>

typedef unsigned long long ticks;

#define JS_SMM_ARGUMENT_ADDR 0x49000e00
#define CMD_REVIRT		2
#define CMD_KPT_CHANGE	3

#define __NR_smm_op 314

unsigned long smm_op(int cmd, void *args)
{
	return syscall(__NR_smm_op,cmd,args);
}
static __inline__ ticks getticks_user(void)
{
	unsigned a, d;
	//asm("cpuid");
	asm volatile("rdtsc" : "=a" (a), "=d" (d));

	return (((ticks)a) | (((ticks)d) << 32));
}
inline void argument_outb(unsigned char v, unsigned short port)
{
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}

int main(void)
{

	uint16_t smi_command_port, smi_status_port, pm1;
	uint8_t smm_enable, smicom_enable;
	uint8_t timer_data, smi_command_data, smi_status_data, eos_data, misc_data, misc_status_data, smi_data;
	uint32_t aaaa;
	unsigned int temp_data = 0;
	unsigned long tick, sum;
	unsigned char c;
	unsigned divisor;
	ticks tick1, tick2, tickh, j;
	int fd;
	int size, temp_size,i;
	unsigned int time = 0;
	char *smihandler;
	FILE *fSMIHandler;
	unsigned short port;
	smi_request_t *req;
	u64	*smm_argument = NULL;
	//ioperm(0x0,0xffff,1);
	printf ("abc1\n");
	iopl(3);

	printf ("abc2\n");

	
	// Get SMI Command Port
	outb(PMIO_SMICOMMAND,PMIO_COMMAND);
	smi_command_port = inw(PMIO_DATA);
	printf("smi_command = %x\n",smi_command_port);


	// Get SMI STATUS Port
	//outb(PMIO_SMISTATUS,PMIO_COMMAND);
	smi_status_port = smi_command_port + 1; //inb(PMIO_DATA);
	printf("smi_status = %x\n",smi_status_port);

	// Get SMM Enable Check
	outb(PMIO_SMMENABLE,PMIO_COMMAND);
	smm_enable = inb(PMIO_DATA);
	printf("smi_enabled ? %d\n",(smm_enable & PMIO_SMMENABLE_BIT) ? 0 : 1);

	// Get SMI Command Enable Check
	outb(PMIO_SMICOMENABLE,PMIO_COMMAND);
	smicom_enable = inb(PMIO_DATA);
	printf("smi_command_port_enabled ? %d\n",(smicom_enable & PMIO_SMICOMENABLE_BIT) ? 1 : 0);

	/*
	stream = (long long)fopen("/dev/mem", "r+");

	if (stream < 0)
	{
		printf("Failed to open /dev/mem\n");
		exit(EXIT_FAILURE);
	}
	
	//req = (smi_request_t *)xc_map_foreign_range(xc_fd, DOMID_SELF, 1024, PROT_WRITE | PROT_READ, SMM_ARGUMENT_ADDR/(4*1024));
	// Jaeseong, 2015-07-28, xc_map_foreign_range --> mmap, for executing smm code in kernel (not on the xen)
	req = (smi_request_t *)mmap((void *)SMM_ARGUMENT_ADDR, 1024, PROT_WRITE | PROT_READ, MAP_FIXED, (int)stream, 0) ;

	if(req == MAP_FAILED) { 
		printf("Could not map memory area, errno: %d\n", errno); 
		exit(EXIT_FAILURE); 
	}
	*/
	fd = open(MEMDEVICE, O_RDWR);

	if(fd < 0) {
		printf("Opening %s failed, errno: %d\n", MEMDEVICE, errno);
		exit(EXIT_FAILURE);
	}
	printf("Successfully opened %s\n", MEMDEVICE);
	
	/*printf ("1\n");
	sleep (1);*/

	req = (smi_request_t *)mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SMM_ARGUMENT_ADDR);
	//smm_argument = (u64*)mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, JS_SMM_ARGUMENT_ADDR);

	/*printf ("2\n");
	sleep (1);*/
	if(req == MAP_FAILED) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}
	/*if(smm_argument == MAP_FAILED) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}*/

	//req->command 					= CMD_ACCOUNT_VCPU;
	//req->command 					= CMD_TEST;
	req->param_test.addr			= 1000;
	req->command = CMD_KPT_CHANGE;
	//*(smm_argument) = CMD_KPT_CHANGE;
	outb(0x2,smi_command_port);
	
	close(fd);

	return 0;
}
