#include <smiutil.h>

/* Basic port I/O */
inline u32 get_core_num(void)
{
	u32 v;
	asm volatile( \
	"movl $0xfee00020, %%esi\n\t" \
	"movl (%%esi), %%ecx\n\t" \
	"shr $24, %%ecx\n\t" \
	"movl %%ecx,%0" : "=a" (v));

	return v;
}
inline void outb(u8 v, u16 port)
{
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}
inline u8 inb(u16 port)
{
	u8 v;
	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

inline void outw(u16 v, u16 port)
{
	asm volatile("outw %0,%1" : : "a" (v), "dN" (port));
}
inline u16 inw(u16 port)
{
	u16 v;
	asm volatile("inw %1,%0" : "=a" (v) : "dN" (port));
	return v;
}
inline u32 inl(u16 port)
{
	u32 v;
	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
}
inline void outl(u32 v, u16 port)
{
	asm volatile("outl %0,%1" : : "a" (v), "dN" (port));
}
inline void rsm(void)
{
	asm volatile("rsm");
}
/*
inline u32 inl(u32 port)
{
	u32 v;
	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
}
*/
inline void io_delay(void)
{
	const u16 DELAY_PORT = 0x80;
	asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}

inline u64 rdmsr(uint32_t msr)
{
	u64 val;
	asm volatile("rdmsr" : "=A" (val) : "c" (msr));
	return val;
}

inline void asm_rdmsr32 (u64 num, u32 *a, u32 *d)
{
	asm volatile ("rdmsr" : "=a" (*a), "=d" (*d) : "c" (num));
}

inline void asm_rdmsr64 (u64 num, u64 *value)
{
	u32 a, d;

	asm_rdmsr32 (num, &a, &d);
	*value = (u64)a | ((u64)d << 32);
}

inline void wrmsr(u32 msr, u64 val)
{
	asm volatile("wrmsr" : : "c" (msr), "A"(val));
}

void clear_smi_status(void)
{
	uint8_t smi_data;

	//outb(PMIO_SMIRESULT,PMIO_COMMAND);
	//outb(0,0xb1);

	/*
	outb(PMIO_SMIRESULT,PMIO_COMMAND);
	smi_data = inb(PMIO_DATA);
	outb(smi_data,PMIO_DATA);

	outb(PMIO_WAKESMI_STATUS,PMIO_COMMAND);
	smi_data = inb(PMIO_DATA);
	outb(smi_data,PMIO_DATA);
	*/

	outb(PMIO_MISCSTATUS,PMIO_COMMAND);
	smi_data = inb(PMIO_DATA);
	outb(smi_data,PMIO_DATA);


	outb(PMIO_SMIRESULT,PMIO_COMMAND);
	outb(0x4,PMIO_DATA);
	outb(PMIO_WAKEIRQ_STATUS,PMIO_COMMAND);
	outb(PMIO_WAKESMI_SERI,PMIO_DATA);




}
void smi_set_eos(void)
{
	uint8_t eos_data;

	//outb(PMIO_SMIRESULT,PMIO_COMMAND);
	//outb(0,0xb1);

	outb(PMIO_EOS,PMIO_COMMAND);
	outb(PMIO_EOS_BIT,PMIO_DATA);
//	eos_data = inb(PMIO_DATA);
//	if (!(eos_data & PMIO_EOS_BIT))
//	{
//		eos_data |= PMIO_EOS_BIT;
//	}
}
void hex_to_string(unsigned long long num, char *str)
{ 
	unsigned long long deg		= 1; 
	int i 						= 0; 
	int radix 					= 16;  
	int cnt 					= 0; 


	if (num == 0)
	{
		*(str++)	= '0';
		*(str++)	= 'x';
		*(str++)	= '0';
		*(str++) 	= '\0'; 
	}
	else
	{

		*(str++)	= '0';
		*(str++)	= 'x';
		while(1)
		{  
			if( (num/deg) > 0) 
				cnt++; 
			else 
				break; 
			deg *= radix; 
		} 

		deg /= radix;  
		
		for(i=0; i<cnt; i++)    
		{  
			if (num / deg > 9) *(str+i) = num/deg -10 + 'a';
			else *(str+i) = num/deg + '0';    
			num -= ((num/deg) * deg);        
			deg /=radix;    
		} 
		*(str+i) = '\0'; 
	}
}  
inline ticks getticks(void)
{
	unsigned a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));

	return (((ticks)a) | (((ticks)d) << 32));
}
