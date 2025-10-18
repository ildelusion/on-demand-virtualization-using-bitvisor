#include <stdio.h>
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE */
#include <fcntl.h>    /* open(), O_RDWR */
#include <errno.h>    /* errno */
#include <sys/mman.h> /* mmap(), munmap(), PROT_READ, PROT_WRITE, MAP_SHARED */
//#include <xenctrl.h>
#include "libSMM.h"

#define DEBUG_DUMP_SMM 1

int main(int argc, char *argv[])
{
	uint64_t smmBase;
	uint64_t smmEnd;
	uint64_t smmAddr;
	uint64_t smmMask;
	int fd,size,result,i;
	long long stream;
	FILE *fSMMHandler;
	unsigned char *vidmem;
	char savePath[100];
	uint8_t cpu;


	if (argc != 2)
	{
		printf("%s [cpu_num]\n",argv[0]);
		exit(1);
	}
	else cpu = atoi(argv[1]);	

	
	if (!check_SMMLock(cpu)) { printf("SMM lock bit is set\n"); exit(1); }
	else printf("SMM lock bit is unset\n"); 

	
	/*
	stream = (long long)fopen("/dev/mem", "r+");

	if (stream < 0)
	{
		printf("Failed to open /dev/mem\n");
		exit(EXIT_FAILURE);
	}
	*/	
	

	smmBase = get_SMMBase(cpu);
	printf("SMMBase = %lx\n",smmBase);

	smmAddr = get_SMMAddr(cpu);
	smmMask = get_SMMMask(cpu);
	printf("SMM_AValid = %d\n",check_AValid(smmMask));
	printf("SMM_AClose = %d\n",check_AClose(smmMask));
	printf("SMM_TValid = %d\n",check_TValid(smmMask));
	printf("SMM_TClose = %d\n",check_TClose(smmMask));
	printf("SMMAddr = %lx\n",smmAddr);
	printf("SMMMASK = %lx\n",smmMask);
	printf("TSegMask = %lx\n",get_TSegMask(smmMask));

	

	if (DEBUG_DUMP_SMM) {
		i = 0;
		if (is_Aseg(smmBase))
		{
			unset_AValid(i);
			set_mtrr_fix_dramen(i);
			set_mtrr_fix(i);
		}
		else
		{
			unset_TValid(i);
		}
	} else {
		if (is_Aseg(smmBase))
		{
			for (i = 0; i < 5; i++)
			{
				unset_AValid(i);
				set_mtrr_fix_dramen(i);
				set_mtrr_fix(i);
			}

		}
		else
		{
			for (i = 0; i < 5; i++)
				unset_TValid(i);
		}
	}


	//vidmem = xc_map_foreign_range(stream, DOMID_SELF, 0x100000, PROT_WRITE | PROT_READ,SMM_TSEG_ADDR/(4*1024));	
	// Jaeseong, 2015-07-28, xc_map_foreign_range --> mmap, for executing smm code in kernel (not on the xen)
	/*
	vidmem= mmap((void*)SMM_TSEG_ADDR, 0x100000, PROT_WRITE | PROT_READ, MAP_FIXED, stream, 0);
	
	if(vidmem == MAP_FAILED) {
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

	vidmem = mmap(NULL, 0x100000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, smmBase);
	if(vidmem == MAP_FAILED) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}

	if(vidmem == NULL) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}
	
	printf("Successfully mapped video memory\n");
	result = sprintf(savePath,"%s_%d",SMM_FILE_PATH,cpu);
	fSMMHandler = fopen(savePath,"w");
	size = fwrite(vidmem,1,0x100000,fSMMHandler);
	printf("Write Memory = %d\n",size);
	fclose(fSMMHandler);
	close(fd);

	if (DEBUG_DUMP_SMM) {
		if (is_Aseg(smmBase))
		{
			i = 0;
			set_AValid(i);
			unset_mtrr_fix_dramen(i);
			unset_mtrr_fix(i);
		}
		else
		{
			set_TValid(i);
		}
	} else {
		if (is_Aseg(smmBase))
		{
			for (i = 0; i < 5; i++)
			{
				set_AValid(i);
				unset_mtrr_fix_dramen(i);
				unset_mtrr_fix(i);
			}

		}
		else
		{
			for (i = 0; i < 5; i++)
				set_TValid(i);
		}
	}

	return 0;
}
