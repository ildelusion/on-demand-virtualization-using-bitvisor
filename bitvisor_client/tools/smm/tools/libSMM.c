#define _XOPEN_SOURCE  500
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include "libSMM.h"

uint64_t get_msr_value (int cpu, uint32_t reg, unsigned int highbit,
                        unsigned int lowbit, int* error_indx)
{
    uint64_t data;
    int fd;
    char msr_file_name[64];
    int bits;
    *error_indx =0;

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_RDONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            //fprintf (stderr, "rdmsr: No CPU %d\n", cpu);
            *error_indx = 1;
            return 1;
        } else if (errno == EIO) {
            //fprintf (stderr, "rdmsr: CPU %d doesn't support MSRs\n", cpu);
            *error_indx = 1;
            return 1;
        } else {
            //perror ("rdmsr:open");
            *error_indx = 1;
            return 1;
            //exit (127);
        }
    }

    if (pread (fd, &data, sizeof data, reg) != sizeof data)
    {
        perror ("rdmsr:pread");
        exit (127);
    }

    close (fd);

    bits = highbit - lowbit + 1;
    if (bits < 64)
    {
        /* Show only part of register */
        data >>= lowbit;
        data &= (1ULL << bits) - 1;
    }

    /* Make sure we get sign correct */
    if (data & (1ULL << (bits - 1)))
    {
        data &= ~(1ULL << (bits - 1));
        data = -data;
    }

    *error_indx = 0;
    return (data);
}

uint64_t set_msr_value (int cpu, uint32_t reg, uint64_t data)
{
    int fd;
    char msr_file_name[64];

    sprintf (msr_file_name, "/dev/cpu/%d/msr", cpu);
    fd = open (msr_file_name, O_WRONLY);
    if (fd < 0)
    {
        if (errno == ENXIO)
        {
            fprintf (stderr, "wrmsr: No CPU %d\n", cpu);
            exit (2);
        } else if (errno == EIO) {
            fprintf (stderr, "wrmsr: CPU %d doesn't support MSRs\n", cpu);
            exit (3);
        } else {
            perror ("wrmsr:open");
            exit (127);
        }
    }

    if (pwrite (fd, &data, sizeof data, reg) != sizeof data)
    {
        perror ("wrmsr:pwrite");
        exit (127);
    }
    close(fd);
    return(1);
}
#define rdmsr(cpu,msr,val) do { int a__; \
		val = get_msr_value (cpu, msr, 63, \
                        0, &a__ ); \
 } while(0); 

#define wrmsr(cpu,msr,val) do { \
		set_msr_value (cpu, msr, val); \
 } while(0); 

int check_SMMLock(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_HWCR,val);
	_DEBUG("MSR_HWCR:%lx\n",val);
	return val & ~SMM_LOCK_BIT;
}
uint64_t get_SMMBase(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_SMMBASE,val);
	_DEBUG("MSR_SMMBASE:%lx\n",val);
	return val;
}
void set_SMMBase(uint8_t cpu, uint64_t val)
{
	wrmsr(cpu,MSR_SMMBASE,val);
	_DEBUG("MSR_SMMBASE:%lx\n",val);
}
void set_mtrr_fix(uint8_t cpu)
{
	uint64_t msr_val = MTRR_FIX_VAL;
	wrmsr(cpu, MSR_FIX_MTRR, msr_val);
}
void unset_mtrr_fix(uint8_t cpu)
{
	uint64_t msr_val = 0;
	wrmsr(cpu, MSR_FIX_MTRR, msr_val);
}
void unset_mtrr_fix_dramen(uint8_t cpu)
{
	uint64_t msrSYSCFG;
	rdmsr(cpu,MSR_SYS_CFG, msrSYSCFG); 
	msrSYSCFG = msrSYSCFG & ~MTRR_FIX_DRAM_EN;
	wrmsr(cpu, MSR_SYS_CFG, msrSYSCFG);
}
void set_mtrr_fix_dramen(uint8_t cpu)
{
	uint64_t msrSYSCFG;
	rdmsr(cpu,MSR_SYS_CFG, msrSYSCFG); 
	msrSYSCFG = msrSYSCFG | MTRR_FIX_DRAM_EN;
	wrmsr(cpu, MSR_SYS_CFG, msrSYSCFG);
}
void unset_TClose(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask & ~SMM_TSEG_CLOSE;
	set_SMMMask(cpu,smmMask);
}
void unset_TValid(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask & ~SMM_TSEG_VALID;
	set_SMMMask(cpu,smmMask);
}
void set_TValid(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask & ~SMM_TSEG_VALID;
	smmMask = smmMask | SMM_TSEG_VALID;
	set_SMMMask(cpu,smmMask);
}
void unset_TDram(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask & ~SMM_TSEG_DRAM;
	set_SMMMask(cpu,smmMask);
}
void set_TDram(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask |= SMM_TSEG_DRAM;
	set_SMMMask(cpu,smmMask);
}
void set_TMask(uint8_t cpu, uint64_t order)
{
	uint64_t smmMask, new;
	smmMask = get_SMMMask(cpu);
	_DEBUG("original:%lx\n",smmMask);
	smmMask = smmMask & ((1 << 15) - 1);
	_DEBUG("original2:%lx\n",smmMask);
	new = SMM_TSEG_MASK_BASE;
	new = new << order;
	_DEBUG("original3:%lx\n",new);
	smmMask |= new;
	_DEBUG("original4:%lx\n",smmMask);
	set_SMMMask(cpu,smmMask);
}
void unset_AValid(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask & ~SMM_ASEG_VALID;
	set_SMMMask(cpu,smmMask);
}
void set_AValid(uint8_t cpu)
{
	uint64_t smmMask;
	smmMask = get_SMMMask(cpu);
	smmMask = smmMask | SMM_ASEG_VALID;
	set_SMMMask(cpu,smmMask);
}
int check_AValid(uint64_t val)
{
	return (val & SMM_ASEG_VALID) > 0;
}
int check_TValid(uint64_t val)
{
	return (val & SMM_TSEG_VALID) > 0;
}
int check_AClose(uint64_t val)
{
	return (val & SMM_ASEG_CLOSE) > 0;
}
int check_TClose(uint64_t val)
{
	return (val & SMM_TSEG_CLOSE) > 0;
}
int is_Aseg(uint64_t addr)
{
	if (addr < SMM_ASEG_ADDR + 0x1ffff)
		return 1;
	else return 0;
}
uint64_t get_SMMAddr(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_SMMADDR,val);
	_DEBUG("MSR_SMMADDR:%lx\n",val);
	return val;
}
void set_SMMAddr(uint8_t cpu, uint64_t val)
{
	wrmsr(cpu,MSR_SMMADDR,val);
	_DEBUG("MSR_SMMADDR:%lx\n",val);
}
uint64_t get_SMMMask(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_SMMMASK,val);
	_DEBUG("MSR_SMMMASK:%lx\n",val);
	return val;
}
void set_SMMMask(uint8_t cpu, uint64_t val)
{
	wrmsr(cpu,MSR_SMMMASK,val);
	_DEBUG("MSR_SMMMASK:%lx\n",val);
}
uint64_t get_TSegMask(uint64_t val)
{
	_DEBUG("%lx\n",val);
	val = val & ~SMM_TSEG_MASK;
	_DEBUG("%lx\n",val);
	return val;
}
uint64_t get_SMITriggerIO(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_SMMTRIG_IO,val);
	_DEBUG("MSR_SMMTRIG_IO:%lx\n",val);
	return val;
}
uint64_t get_SMIIOTrapControl(uint8_t cpu)
{
	uint64_t val;
	rdmsr(cpu,MSR_SMITRAP_CONTROL,val);
	_DEBUG("MSR_SMITRAP_CONTROL:%lx\n",val);
	return val;
}
uint64_t set_SMIIOTrapControl(uint8_t cpu)
{
	uint64_t val;
	wrmsr(cpu,MSR_SMITRAP_CONTROL,SMM_TRAP_ENABLE);
	rdmsr(cpu,MSR_SMITRAP_CONTROL,val);
	_DEBUG("MSR_SMITRAP_CONTROL:%lx\n",val);
	return val;
}
