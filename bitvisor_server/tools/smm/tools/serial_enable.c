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
#include <stdio.h>

// Serial related function. Reference: OSDev
/*
#define SERIAL_PORT 0x3f8
void init_serial(void){
	//outb(0x00, SERIAL_PORT + 1);	// Disable all interrupts.
	outb(0x0f, SERIAL_PORT + 1); 	// Enable all interrupts.
	outb(0x80, SERIAL_PORT + 3);
	outb(0x01, SERIAL_PORT + 0);
	outb(0x00, SERIAL_PORT + 1);
	outb(0x03, SERIAL_PORT + 3);
	outb(0xc7, SERIAL_PORT + 2);
	outb(0x0b, SERIAL_PORT + 4);
}
int serial_received(void){
	return inb (SERIAL_PORT+5) & 1;
}
char read_serial(){
	while (serial_received() == 0);
	return inb(SERIAL_PORT);
}

int is_transmit_empty(){
	return inb(SERIAL_PORT + 5) & 0x20;
} 

void write_serial(char a){
	while (is_transmit_empty() == 0);
	outb (a, SERIAL_PORT);
}
*/

int main(int argc, char *argv[])
{
	int fd;
	smi_request_t *req;
	uint8_t eos_data;

	iopl(3);
	
	/* Clear WAKE IRQ */

	outb(PMIO_WAKEIRQ,PMIO_COMMAND);
	eos_data = inb (PMIO_DATA);
	printf("WAKEIRQ: %x\n", eos_data); 

	eos_data = inb (PMIO_DATA);
	eos_data = 0;
	outb(eos_data,PMIO_DATA );
	eos_data = inb (PMIO_DATA);
	printf("WAKEIRQ: %x\n", eos_data); 
	
	/* Enable Serial IRQ */
	outb(PMIO_WAKEIRQ,PMIO_COMMAND);
	if (argc == 1)
	{
		outb(PMIO_WAKESMI_SERI,PMIO_DATA );
		eos_data = inb (0x3f8 + 0x1);	// Default serial port: 0x3f8 ttyS0. 0x3f8+1=interrupt enable.
		printf("Interrupt Enable: %x\n", eos_data); 
		outb(1,0x3f8 + 0x2);	// 0x3f8+2 = enable FIFO
		outb(0x5,0x3f8 + 0x1);	// enable interrupts(?) 0x5?
	}
	else 
		outb(0x0,PMIO_DATA );
	eos_data = inb (PMIO_DATA);
	printf("WAKEIRQ: %x\n", eos_data); 
	
	fd = open(MEMDEVICE, O_RDWR);

	if(fd < 0) {
		printf("Opening %s failed, errno: %d\n", MEMDEVICE, errno);
		exit(EXIT_FAILURE);
	}
	printf("Successfully opened %s\n", MEMDEVICE);
	
	req = (smi_request_t *)mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SMM_ARGUMENT_ADDR);

	req->command 					= CMD_TEST;
	if(req == NULL) { 
		printf("Could not map memory area, errno: %d\n", errno); 
		exit(EXIT_FAILURE); 
	}
	return 0;
}
