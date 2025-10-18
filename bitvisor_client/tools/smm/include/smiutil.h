#ifndef SMM_UTIL_H
#define SMM_UTIL_H

#include <types.h>
#include <smm.h>
#include <serial.h>
#include <lib.h>
#include <define.h>


#define DEBUG

#define print(z, x) {  \
					char test_print[20]; \
					new_puts(z); \
					hex_to_string (x, test_print); \
					new_puts(test_print); \
					}

#define println(z, x) { \
					print(z, x); \
					new_putchar('\n'); \
					}
	
#ifdef NOT_DEBUG

#define debug_print(z, x) 

#define debug_println(z, x) 

#else

#define debug_print(z, x) print(z, x)

#define debug_println(z, x) println(z, x)
/*
#define _debug_print(z, x) {  \
					char test_print[20]; \
					new_puts(z); \
					hex_to_string (x, test_print); \
					new_puts(test_print); \
					}

#define debug_print(z, x) {  \
					_debug_print(z, x) \
					}


#define debug_println(z, x) { \
					_debug_print(z, x); \
					new_putchar('\n'); \
					}
					*/
	
#endif

#define wmb()	__asm__ __volatile__ ("": : :"memory")

typedef unsigned long long ticks;

inline void outb(u8 v, u16 port);
inline u8 inb(u16 port);
inline void outw(u16 v, u16 port);
inline u16 inw(u16 port);
inline void outl(u32 v, u16 port);
//inline u32 inl(u32 port);
inline void io_delay(void);
inline u64 rdmsr(u32 msr);
inline void wrmsr(u32 msr, u64 val);
inline void rsm(void);
void smi_set_eos(void);
void clear_smi_status(void);
void hex_to_string(unsigned long long num, char *str);
inline ticks getticks(void);

#endif


