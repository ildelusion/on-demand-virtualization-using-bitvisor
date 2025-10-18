#include <stdio.h>
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE */
#include <fcntl.h>    /* open(), O_RDWR */
#include <errno.h>    /* errno */
#include <sys/mman.h> /* mmap(), munmap(), PROT_READ, PROT_WRITE, MAP_SHARED */
#include <string.h>   /* memcpy() */
#include "libSMM.h"
//#include <xenctrl.h>
#include <unistd.h>	// JSIM

#define SINGLE_CORE 1
#define JS_DEBUG 1

int main(int argc, char *argv[])
{
	uint64_t smmBase = 0;
	uint64_t smmEnd  = 0;
	uint64_t smmAddr = 0;
	uint64_t smmMask = 0;

	int fd, b;
	int size, temp_size, i;
	char *smihandler;
	FILE *fSMIHandler;
	unsigned char *vidmem = NULL;
	uint8_t cpu = 5;

	if (SINGLE_CORE) {
		i = 0;
		if (!check_SMMLock(i)) { printf("[CPU:%d]SMM lock bit is set\n",i); exit(1); }

		unset_TValid(i);
		unset_AValid(i);
		

		smmBase = SMM_TSEG_ADDR + (SMM_TSEG_OFFSET*i);
		smmAddr = smmBase;


		printf("[CPU:%d]SMMBase = %lx\n",i,smmBase);
		set_SMMBase(i,smmBase);
		set_SMMAddr(i,smmAddr);

	} else {
		for (i = 5; i >= 0; i--)
		{
			if (!check_SMMLock(i)) { printf("[CPU:%d]SMM lock bit is set\n",i); exit(1); }

			unset_TValid(i);
			unset_AValid(i);
			smmBase = SMM_TSEG_ADDR + (SMM_TSEG_OFFSET*i);
			smmAddr = smmBase;
			printf("[CPU:%d]SMMBase = %lx\n",i,smmBase);
			set_SMMBase(i,smmBase);
			set_SMMAddr(i,smmAddr);
		}
	}

	fd = open(MEMDEVICE, O_RDWR);


	if(fd < 0) {
		printf("Opening %s failed, errno: %d\n", MEMDEVICE, errno);
		exit(EXIT_FAILURE);
	}
	printf("Successfully opened %s\n", MEMDEVICE);

	vidmem = mmap(NULL, MAPPEDAREASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, smmBase);
	//vidmem = mmap(NULL, MAPPEDAREASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x50000000);
	/*if (JS_DEBUG) {
		printf ("mmap success\n");
		sleep (1);
	}*/

	if(vidmem == MAP_FAILED) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}


	if(vidmem == NULL) {
		printf("Could not map memory area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}
	printf("Successfully mapped video memory %p\n", vidmem);

	fSMIHandler = fopen(SMI_FILE_PATH,"r");
	smihandler = vidmem;


	b = 0;
	while (size = fread(smihandler ,1,1024,fSMIHandler))
	{
		smihandler+= size;
		b++;
		printf("copy...%p %d\n", smihandler, b);
	}

	fclose(fSMIHandler);

	
	/*if(munmap(vidmem,  MAPPEDAREASIZE) < 0) {
		printf("Could not release mapped area, errno: %d\n", errno);
		exit(EXIT_FAILURE);
	}*/
	// swjin's kernel code end
	
	if (SINGLE_CORE) {
		i = 0;
		set_TMask(i,SMM_TSEG_MASK_SIZE);
		set_TDram(i);
		set_TValid(i);

	} else {
		for (i = 5; i >= 0; i--)
		{
			set_TMask(i,SMM_TSEG_MASK_SIZE);
			set_TDram(i);
			set_TValid(i);
		}
	}

	close(fd);

//	fSMMHandler = fopen(SMM_FILE_PATH,"w");
//	size = fwrite(vidmem + 0x8000,1,1024*1024 - 0x8000,fSMMHandler);
//	printf("Write Memory = %d\n",size);
//	fclose(fSMMHandler);
//	close(fd);

	return 0;
}
