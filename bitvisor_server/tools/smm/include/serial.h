#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

#define DEFAULT_SERIAL_PORT ((u16)0x3f8) /* ttyS0 */

#define XMTRDY          0x20

#define DLAB		0x80

#define TXR             0       /*  Transmit register (WRITE) */
#define RXR             0       /*  Receive register  (READ)  */
#define IER             1       /*  Interrupt Enable          */
#define IIR             2       /*  Interrupt ID              */
#define FCR             2       /*  FIFO control              */
#define LCR             3       /*  Line control              */
#define MCR             4       /*  Modem control             */
#define LSR             5       /*  Line Status               */
#define MSR             6       /*  Modem Status              */
#define DLL             0       /*  Divisor Latch Low         */
#define DLH             1       /*  Divisor latch High        */

#define DEFAULT_BAUD ((unsigned int)115200)
#define BASE_BAUD (1843200/16)



void new_putchar(unsigned char ch);
void new_puts(const unsigned char *str);
void serial_init(u16 port, unsigned int baud);
unsigned int probe_baud(u16 port);
inline void cpu_relax(void);

#endif /* SERIAL_H */
