#include <serial.h>
#include <smm.h>
#include <lib.h>
#include <xmalloc.h>
#include <smiutil.h>
#include <mm.h>
#include <define.h>
#include <smm_types.h>

static void	*free_mem_ptr;
static void *free_mem_end_ptr;
volatile static int started = 0;

void smi_handler(void)
{
	size_t length;
	int ret;
	
	smi_request_t *req	= (smi_request_t *)(SMM_ARGUMENT_ADDR);

	if (started == 0)
	{
		//#ifndef NOT_DEBUG
		serial_init(DEFAULT_SERIAL_PORT,DEFAULT_BAUD);
		//#endif
		free_mem_ptr 		= (void *)(HEAP_START) ;
		free_mem_end_ptr 	= (void *)(HEAP_START + HEAP_SIZE);

		init_page_allocator((u64)free_mem_ptr, (u64)free_mem_end_ptr);
		init_free_list();
		started = 1;
	}
	
	
	if (req != NULL)
	{
		switch (req->command)
		{

			case CMD_TEST :
				println("hello World ", req->param_test.addr);
				break;
		}
	}

	
	clear_smi_status();
	smi_set_eos();
	
}
