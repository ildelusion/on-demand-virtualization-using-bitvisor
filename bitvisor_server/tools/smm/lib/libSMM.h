#ifndef SMM_FUNCTIONS_H
#define SMM_FUNCTIONS_H

#include <inttypes.h>
#include <sys/io.h>   /* inl(), outl()  */
#include "define.h"

#define DEBUG 
//#undef DEBUG 
#ifdef DEBUG
    #define _DEBUG(fmt, args...) printf("DEBUG::%s:%s:%d: "fmt, __FILE__, __FUNCTION__, __LINE__, args)                                                                          
#else
    #define _DEBUG(fmt, args...)
#endif
int check_SMMLock(uint8_t cpu);
uint64_t get_SMMBase(uint8_t cpu);
void set_SMMBase(uint8_t cpu, uint64_t val);
uint64_t get_SMMAddr(uint8_t cpu);
void set_SMMAddr(uint8_t cpu, uint64_t val);
uint64_t get_SMMMask(uint8_t cpu);
void set_SMMMask(uint8_t cpu, uint64_t val);
uint64_t get_SMITriggerIO(uint8_t cpu);
uint64_t get_TSegMask(uint64_t val);
uint64_t set_SMIIOTrapControl(uint8_t cpu);


uint64_t get_TSegMask(uint64_t val);
void set_TDram(uint8_t cpu);
void set_TMask(uint8_t cpu, uint64_t val);

void set_mtrr_fix(uint8_t cpu);
void unset_mtrr_fix(uint8_t cpu);
void set_mtrr_fix_dramen(uint8_t cpu);
void unset_mtrr_fix_dramen(uint8_t cpu);

void set_TValid(uint8_t cpu);
void unset_TValid(uint8_t cpu);
void set_AValid(uint8_t cpu);
void unset_AValid(uint8_t cpu);

int check_AValid(uint64_t val);
int check_TValid(uint64_t val);
int check_AClose(uint64_t val);
int check_TClose(uint64_t val);
int is_Aseg(uint64_t addr);
#endif /* SMM_FUNCTIONS_H */
