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

#define JS_SMM_ARGUMENT_ADDR 0x49000e00
#define CMD_KPT_CHANGE	0

int main(void)
{
	uint16_t smi_command_port, smi_status_port, pm1;
	uint8_t smm_enable, smicom_enable;
	int fd;
	unsigned int time = 0;
	char *smihandler;
	FILE *fSMIHandler;
	unsigned short port;
	smi_request_t *req;
	//ioperm(0x0,0xffff,1);
	iopl(3);

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

	fd = open(MEMDEVICE, O_RDWR);

	if(fd < 0) {
		printf("Opening %s failed, errno: %d\n", MEMDEVICE, errno);
		exit(EXIT_FAILURE);
	}
	printf("Successfully opened %s\n", MEMDEVICE);
	
	req = (smi_request_t *)mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SMM_ARGUMENT_ADDR);

	if(req == MAP_FAILED) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}
	//req->command 					= 22;
	req->command 					= CMD_TEST;
	//req->command 					= CMD_ACCOUNT_VCPU;
	req->param_test.addr			= 1000;
	//outb(0x2,smi_command_port);
	
	close(fd);

	return 0;
}
