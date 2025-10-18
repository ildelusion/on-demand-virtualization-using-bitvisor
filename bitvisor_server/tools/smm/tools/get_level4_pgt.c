#include <stdio.h>	// fgets, popen
#include <errno.h>
#include <string.h>

#define BUF 256

int main()
{
	FILE *system_map_fp = NULL;
	char unameCommand[20] = "uname -r";
	FILE *uname_fp = NULL;
	size_t readSize = 0;
	char kernel_version[20];
	char system_map_kernel_version[50] = "/boot/System.map-";
	char str[BUF];
	char *target_str = "init_level4_pgt";
	char init_level4_pgt[8];

	uname_fp = popen(unameCommand, "r");
	if (!uname_fp)
	{
		printf("error [%d:%s]\n", errno, strerror(errno));
		return -1;
	}
	
	readSize = fread((void*)kernel_version, sizeof(char), 19, uname_fp);

	if(readSize == 0)
	{
		pclose(uname_fp);
		printf("error [%d:%s]\n", errno, strerror(errno));
		return -1;
	}
	
	pclose(uname_fp);
	kernel_version[readSize-1] = 0;

	//printf ("%s\n", kernel_version);
	
	strcat(system_map_kernel_version, kernel_version);	
	system_map_fp = fopen(system_map_kernel_version, "r");

	while (1)
	{
		fgets(str, BUF, system_map_fp);
		if (strstr(str, target_str) != 0)
		{
			//printf("%s\n", str);
			break;
		}
	}

	fclose(system_map_fp);

	//get physical address
	strncpy(init_level4_pgt, &str[9], 7);
	init_level4_pgt[7] = 0;
	printf("%s\n", init_level4_pgt);
	
	return 0;
}
