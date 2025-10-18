#include <serial.h>
#include <smiutil.h>

static u16 early_serial_base;

inline void cpu_relax(void)
{
	__asm__ __volatile__("rep;nop": : :"memory");
}


/*
 * These functions are in .inittext so they can be used to signal
 * error during initialization.
 */

void serial_putchar(unsigned char ch)
{
	unsigned timeout = 0xffff;

	while ((inb(early_serial_base + LSR) & XMTRDY) == 0 && --timeout)	// LSR == 5, XMTRDY == 0x20
		cpu_relax();

	outb(ch, early_serial_base + TXR);
}


/*
void bios_putchar(int ch)
{
	struct biosregs ireg;

	initregs(&ireg);
	ireg.bx = 0x0007;
	ireg.cx = 0x0001;
	ireg.ah = 0x0e;
	ireg.al = ch;
	intcall(0x10, &ireg, NULL);
}
*/

void new_putchar(unsigned char ch)
{
	if (ch == '\n')
		new_putchar('\r');	

	if (early_serial_base != 0)
		serial_putchar(ch);
}

void new_puts(const unsigned char *str)
{
	while (*str)
		new_putchar(*str++);
}



void serial_init(u16 port, unsigned int baud)
{
	unsigned char c;
	unsigned divisor;
	

	outb(0, port + IER);	// 1
	
	outb(0x3, port + LCR);	// 3
	outb(0, port + FCR);	// 2 FIFO
	outb(0x3, port + MCR);	// 4 Modem control

	divisor	= 115200 / baud;	// default == 1
	c = inb(port + LCR);
	outb(c | DLAB, port + LCR);
	outb(divisor & 0xff, port + DLL);	//0
	outb((divisor >> 8) & 0xff, port + DLH);	//1
	outb(c & ~DLAB, port + LCR);
	
	early_serial_base = port;

}

unsigned int probe_baud(u16 port)
{
	unsigned char lcr, dll, dlh;
	unsigned int quot;

	lcr = inb(port + LCR);
	outb(lcr | DLAB, port + LCR);
	dll = inb(port + DLL);
	dlh = inb(port + DLH);
	outb(lcr, port + LCR);
	quot = (dlh << 8) | dll;

	return BASE_BAUD / quot;
}
