#ifndef SMM_TYPES_H
#define SMM_TYPES_H

#include <types.h>

enum smm_contants {

	SUCCESS = 0,
	ERR_FAIL = -1,
	ERR_NOT_FOUND = -2,	
	ERR_NULL = -3,	
	ERR_EXIST = -4,	

	CMD_CREATE_DOM = 1,
	CMD_DELETE_DOM,
	CMD_ALLOC_NPT,
	CMD_DEALLOC_NPT,
	CMD_PAGE_MAP,
	CMD_PAGE_UNMAP,
	CMD_SET_NPT,
	CMD_CREATE_VMCB,
	CMD_DELETE_VMCB,
	CMD_COPY_SHADOW,
	CMD_COPY_VMCB,
	CMD_ACCOUNT_VCPU_SET,
	CMD_ACCOUNT_RESET,
	CMD_ACCOUNT_VCPU,
	CMD_ACCOUNT_MEM,
	CMD_ACCOUNT_CACHE,
	CMD_PRINT_ACCOUNT_VCPU,
	CMD_GHOST_VCPU,
	CMD_GHOST_VCPU_RELEASE,
	CMD_PRINT_ALL_ACCOUNT_VCPU,
	CMD_ACCOUNT_VCPU_SAVE,

	CMD_TEST = 99,
	CMD_CREATE_OST,
	CMD_LIST_PAGE,
	CMD_LIST_OST,
	CMD_LIST_DOMAIN,
	CMD_LIST_MFN,
	CMD_DUMP_HSAVE,

};

typedef struct smi_f_test
{
	unsigned long addr;
} f_param_test ;



typedef struct smi_request {
	unsigned int command;
	union {
		f_param_test param_test;
	};
} smi_request_t;

#endif
