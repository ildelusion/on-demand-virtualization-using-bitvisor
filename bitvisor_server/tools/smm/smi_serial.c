#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>    /* errno */
#include <sys/mman.h> /* mmap(), munmap(), PROT_READ, PROT_WRITE, MAP_SHARED */
#include <sys/syscall.h> 
#include <unistd.h> 

#include <sys/time.h>
#include <time.h>
#include <stdio.h>


typedef unsigned long long ticks;


#define END_TIME 60		// second
#define INTERVAL 100	// micro-second

static __inline__ ticks getticks_user(void)
{
	unsigned a, d;
	//asm("cpuid");
	asm volatile("rdtsc" : "=a" (a), "=d" (d));

	return (((ticks)a) | (((ticks)d) << 32));
}
inline void outb(unsigned char v, unsigned short port)
{
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}
inline unsigned char inb(unsigned short port)
{
    unsigned char v;
    asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
    return v;
}


int main(int argc, char *argv[])
{

	unsigned long tick, sum;
	unsigned long sleep_time;
	unsigned long io_time;
	ticks tick1, tick2, tickh, j;
	unsigned int i;
	int end_time = 0;
	int interval = 0;

	iopl(3);

	end_time = atoi(argv[1]);
	interval = atoi(argv[2]);

	printf("end_time = %d, interval = %d\n",end_time,interval);
	tick1 = getticks_user();
 
	while (((getticks_user() - tick1)/(3403.504*1000*1000)) < end_time)
	{
		sleep_time = rand() % (interval*2);
		tick2 = getticks_user();
		outb(2,0x3f8);
		io_time = (getticks_user() - tick2)/3403.504;
		if (sleep_time > io_time)
			sleep_time -= io_time;
		usleep(sleep_time);
	}
	
	return 0;
}
