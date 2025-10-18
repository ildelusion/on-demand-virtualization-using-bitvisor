/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.  * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of and a contribution to the lwIP TCP/IP stack.
 *
 * Credits go to Adam Dunkels (and the current maintainers) of this software.
 *
 * Christiaan Simons rewrote this file to get a more stable echo example.
 */

/**
 * @file
 * TCP echo server example using raw API.
 *
 * Echos all bytes sent by connecting client,
 * and passively closes when client is done.
 *
 */
#include <core/mm.h>		// to mapmem
#include <core/string.h>	// to memcpy
#include <core/process.h>	// about msg
#include <core/time.h>
#include <core/mmio.h>
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "biform-net-protocol/msg_protocol.h"

#if LWIP_TCP

#if 0
#define printd(X...) do {printf (X); } while (0)
#else
#define printd(X...)
#endif

// time measuring debugging
#if 1
#define printd_t(X...) do {printf (X); } while (0)
#else
#define printd_t(X...)
#endif

#define GRAN_32KB
//#define CONFIG_POSTCOPY_DBG
//#define CONFIG_COUNT_BACKGROUND_PRESSURE
//#define CONFIG_REDIS_DEBUG

//#define POSTCOPY_DBG 1
#define POSTCOPY_DBG 0
#define DBGPC if (POSTCOPY_DBG) printf

//#define SIZE_OF_SENDBUF 512
#define SIZE_OF_SENDBUF 1048	// MAX: MTU-40 = 1500-40
// SIZE_SENDBUF_DATA 1024 in msg_protocol.h
#define SIZE_OF_METADATA 24

//#define NUM_OF_MEM_AREA 6
#define NUM_OF_MEM_AREA 7
//#define BITARRAY 2176		// 16KB migraion unit
//#define BITARRAY 64	// 1MB migration unit, 4GB memory
//#define BITARRAY 320	// 1MB migration unit, 16GB memory (256 + 64)

#ifdef GRAN_32KB

#define MIG_UNIT 15
#define SHIFT (MIG_UNIT + 6)
#define BITARRAY ((1UL << (34 - MIG_UNIT - 6)) + 1024)     // 2^23 = 16GB, 2^6 = 64-bit array
// You have to cover up to 17.85GB
#define ARRAY_MULTIPLIER (1UL << MIG_UNIT)

#else	// GRAN_32KB

#define BITARRAY 2400	// 1MB migration unit, 16GB memory (256 + 64)
//#define SHIFT 26
#define SHIFT 23
//#define MIG_UNIT 20
#define MIG_UNIT 17
#define ARRAY_MULTIPLIER 0x20000

#endif	// GRAN_32KB

//#define BATCH_SIZE 1024
//#define BATCH_SIZE 128
#define BATCH_SIZE 32
#define EXPR_NUM 300

static struct tcp_pcb *echo_pcb;
static struct tcp_pcb *echo_server_pcb;
// JSIM's counter
static u64 recv_count_for_cpu;
static bool request_is_sent;
// JSIM's vmem_for_cpu
static u64* vmem_for_cpu;
// JSIM's memory list
struct usable_mem {
	u64 start_addr;
	u64 end_addr;
};

static char send_buf[SIZE_OF_SENDBUF];


/*static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},	// virtio ring is included
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
};*/

static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9c6f9},
	{0x100000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},	// virtio ring is included
	{0x86b47000, 0x86b9bfff},
	{0x86fbf000, 0x86fd4fff},
	{0x87fcd000, 0x87ffffff},
	{0x100000000, 0x4767fffff},
};

/*
// before 2022-03-21 
static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},	// virtio ring is included
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	{0x100000000, 0x4767fffff},
};
*/

static unsigned long num_of_recv[NUM_OF_MEM_AREA];
static unsigned long num_of_recved[NUM_OF_MEM_AREA];
static u64* usable_vmem_list[NUM_OF_MEM_AREA];
static unsigned int mem_area_num;
static long total_num_of_recv;
static long total_num_of_recved;

static u64 bitarray[BITARRAY];
static bf_msg_t msg;
static bf_msg_t recv_msg;
u64 rt1, rt2;

enum echo_states
{
	ES_NONE = 0,
	ES_ACCEPTED,
	ES_RECEIVED,
	ES_CLOSING
};

struct echo_state
{
	u8_t state;
	u8_t retries;
	struct tcp_pcb *pcb;
	/* pbuf (chain) to recycle */
	struct pbuf *p;
};

static err_t echo_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t echo_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void echo_error(void *arg, err_t err);
static err_t echo_poll(void *arg, struct tcp_pcb *tpcb);
static err_t echo_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void echo_send(struct tcp_pcb *tpcb, struct echo_state *es);
static void echo_close(struct tcp_pcb *tpcb, struct echo_state *es);
static int tcp_send_msg (struct tcp_pcb *pcb);
static err_t request_sent(void *arg, struct tcp_pcb *pcb, u16_t len);

// JSIM add get_bitarray, set_bitarray
// for postcopy migration
// 2018-02-18
// check whether gphys is migrated or not
unsigned int get_bitarray(u64 gphys)
{
	unsigned int pos = gphys >> SHIFT;	// SHIFT == 20
	printd("pos: %d\n", pos);
	return (bitarray[pos] >> ((gphys >> MIG_UNIT) & 0x3f)) & 1;	// MIG_UNIT == 14
}

void set_bitarray(u64 gphys)
{
	unsigned int pos = gphys >> SHIFT;
	bitarray[pos] |= (1LL << ((gphys >> MIG_UNIT) & 0x3f));
}

static void print_vmrun_time (void)
{
	int i = 0;
	printf ("vmrun_time\n");
	for (i = 0; i < TIME_MEASURE; i++) {
		printf ("%d\n", vmrun_time[i]);
	}
	//time_measure = false;	// error
}

/*
static void print_root_cause_overhead (void)
{
	int i = 0;
	
	printf ("server print start\n");
	for (i = 0; i < 4; i++) {
		printf ("t_start[%d] %ld\n", i, t_start[i]);
	}
	for (i = 0; i < 5; i++) {
		printf ("t_margin[%d] %ld\n", i, t_margin[i]);
	}
	for (i = 0; i < 2; i++) {
		printf ("t_guest_start[%d] %ld\n", i, t_guest_start[i]);
	}
	for (i = 0; i < EXPR_NUM; i++) {
		printf ("t_vm[%d][0] %ld\n", i, t_vm[i][0]);
		printf ("t_vm[%d][1] %ld\n", i, t_vm[i][1]);
	}
	for (i = 0; i < EXPR_NUM; i++) {
		printf ("t_exit_ept[%d] %ld\n", i, t_exit_ept[i]);
	}
	for (i = 0; i < EXPR_NUM; i++) {
		printf ("t_req[%d][0] %ld\n", i, t_req[i][0]);
		printf ("t_req[%d][1] %ld\n", i, t_req[i][1]);
	}
	printf ("server data start. CPU ~ margins recv\n");
	printf ("ready time %ld\n", t_start[1] - t_start[0]);
	printf ("CPU recvTime %ld\n", t_start[3] - t_start[2]);
	for (i = 0; i < 5; i++) {
		printf ("t_margin[%d] %ld\n", i, t_margin[i]);	// print one more time here
	}
	printf ("margins recv %ld\n", t_guest_start[0] - t_start[3]);
	printf ("buff_mem recv ~ vmmcall end %ld\n", t_guest_start[1] - t_guest_start[0]);
	printf ("first vmrun %ld\n", t_vm[0][0] - t_guest_start[1]);
	printf ("plus upper two digits %ld\n", t_vm[0][0] - t_guest_start[0]);
	for (i = 0; i < EXPR_NUM - 1; i++) {
		printf ("%d try\n", i);
		printf ("vmrun~exit %ld\n", t_vm[i][1] - t_vm[i][0] - t_exit_ept[i]);
		printf ("exit~req %ld\n", t_req[i][0] - t_vm[i][1] + t_exit_ept[i]);
		printf ("req~recv %ld\n", t_req[i][1] - t_req[i][0]);
		printf ("recv~vmrun %ld\n", t_vm[i+1][0] - t_req[i][1]);
	}
}*/

	void
echo_server_init (int port)
{
	// JSIM's variable
	int i = 0;
	long mem_start = 0;
	long mem_len = 0;

	echo_pcb = tcp_new();
	printf ("JSIM: echo server init\n");
	if (echo_pcb != NULL)
	{
		err_t err;

		err = tcp_bind(echo_pcb, IP_ADDR_ANY, port);
		if (err == ERR_OK)
		{
			echo_pcb = tcp_listen(echo_pcb);
			tcp_accept(echo_pcb, echo_accept);
			printf ("JSIM: tcp accept called\n");

			/* JSIM 
			 * 20170322 22:31
			 * memory map for cpu states
			 * set num_of_recv and memory map */
			recv_count_for_cpu = 0;
			vmem_for_cpu = (long*)mapmem(MAPMEM_HPHYS | MAPMEM_WRITE, 0x49005000, 1024);

			for(i = 0; i < NUM_OF_MEM_AREA; i++) {
				mem_start = usable_mem_list[i].start_addr;
				mem_len = usable_mem_list[i].end_addr - usable_mem_list[i].start_addr + 1;
				usable_vmem_list[i] = (long*)mapmem(MAPMEM_HPHYS | MAPMEM_WRITE, mem_start, mem_len);
				num_of_recv[i] = mem_len/SIZE_SENDBUF_DATA;	// count have to be received
				num_of_recved[i] = 0;
			}
			mem_area_num = 0;
			total_num_of_recved = 0;
			total_num_of_recv = 0;
			bf_msg_init_msg(&msg);
			bf_msg_init_msg(&recv_msg);
			for(i = 0; i < NUM_OF_MEM_AREA; i++) {
				total_num_of_recv += num_of_recv[i];
			}
		}
		else 
		{
			/* abort? output diagnostic? */
		}
	}
	else
	{
		/* abort? output diagnostic? */
	}
}

	static err_t
echo_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	err_t ret_err;
	struct echo_state *es;

	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);
	printf ("JSIM: echo accept called.\n");

	/* commonly observed practive to call tcp_setprio(), why? */
	tcp_setprio(newpcb, TCP_PRIO_MIN);

	es = (struct echo_state *)mem_malloc(sizeof(struct echo_state));
	if (es != NULL)
	{
		es->state = ES_ACCEPTED;
		es->pcb = newpcb;
		es->retries = 0;
		es->p = NULL;
		/* pass newly allocated es to our callbacks */
		tcp_arg(newpcb, es);
		tcp_recv(newpcb, echo_recv);
		tcp_err(newpcb, echo_error);
		tcp_poll(newpcb, echo_poll, 0);
		ret_err = ERR_OK;
		printf ("JSIM: echo accept called. ERR_OK\n");
		// JSIM 2017-07-13
		echo_server_pcb = newpcb;
	}
	else
	{
		ret_err = ERR_MEM;
	}
	return ret_err;  
}

static void set_mem_area_num (long recved_addr)
{
	if (recved_addr >= usable_mem_list[6].start_addr) {
		mem_area_num = 6;
	} else if (recved_addr <= usable_mem_list[0].end_addr) {
		mem_area_num = 0;
	} else if (recved_addr <= usable_mem_list[1].end_addr) {
		mem_area_num = 1;
	} else if (recved_addr <= usable_mem_list[2].end_addr) {
		mem_area_num = 2;
	} else if (recved_addr <= usable_mem_list[3].end_addr) {
		mem_area_num = 3;
	} else if (recved_addr <= usable_mem_list[4].end_addr) {
		mem_area_num = 4;
	} else {
		mem_area_num = 5;
	}
}

void 
echo_server_reply_request (ulong gphys)
{
	if (!get_bitarray(gphys)) {
		static int i = 0;
		// measuring time get_acpi_time
		//static int j = 0;
		//get_acpi_time (&t_vm[j][1]);
		//t_exit_ept[i] = t_vm[j][1] - t_exit;
		//j++;
		//ept_recv_flag = true;	// 2018-11-12 for measuring time

		DBGPC("send request, gphys: %lx\n", gphys);
		bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_REQ_MEM);
		msg.meta.data_mem.addr_to_migrate = gphys;

		tcp_sent(echo_server_pcb, request_sent);

		//get_acpi_time(&t_req[i++][0]);
		tcp_write (echo_server_pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_COPY);
		tcp_output(echo_server_pcb);
		//19-04-08 for full bandwidth
		//reception = true;
#ifdef CONFIG_COUNT_BACKGROUND_PRESSURE
		request_is_sent = true;
		num_request_sent++;
		request_sent_t = get_cpu_time();
#endif
	} else {
		//printf("already recved\n");
		ept_recved = true;
	}
}

void
echo_server_postcopy_start ()
{
	int ret_tcp_send = -1;
	printd_t ("(%s)\n", __func__);
	bf_msg_set_proto_id (&msg, PROTO_ID_META_START_POSTCOPY);
	tcp_sent (echo_server_pcb, NULL);
	//get_acpi_time (&t_start[1]);
	tcp_write (echo_server_pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_COPY);
	tcp_output(echo_server_pcb);
}

static void set_multiple_bitarray (u64 start_addr, u64 end_addr)
{
	u64 size_area = (end_addr - start_addr + 1) >> MIG_UNIT;
	for (int i = 0; i < size_area; i++) 
	{    
		set_bitarray((start_addr & 0xfffffffffffe0000) + i * ARRAY_MULTIPLIER);
	} 
}

	static err_t
echo_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	struct echo_state *es;
	err_t ret_err;

	LWIP_ASSERT("arg != NULL",arg != NULL);
	es = (struct echo_state *)arg;
	if (p == NULL)
	{
		printf("echo-server.c: p == NULL\n");
		/* remote host closed connection */
		es->state = ES_CLOSING;
		if(es->p == NULL)
		{
			printf("echo_close\n");
			/* we're done sending, close it */
			echo_close(tpcb, es);
		}
		else
		{
			printf("we're not done yet\n");
			/* we're not done yet */
			tcp_sent(tpcb, echo_sent);
			echo_send(tpcb, es);
		}
		ret_err = ERR_OK;
	}
	else if(err != ERR_OK)
	{
		printf("echo-server.c: err != ERR_OK\n");
		/* cleanup, for unkown reason */
		if (p != NULL)
		{
			es->p = NULL;
			pbuf_free(p);
		}
		ret_err = err;
	}
	else if(es->state == ES_ACCEPTED)
	{
		int proto_id;
		int d;
		struct msgbuf mbuf;
		int ret = -1;
		unsigned long array[2];
		/* first data chunk in p->payload */
		es->state = ES_RECEIVED;
		/* store reference to incoming pbuf (chain) */
		es->p = p;
		/* install send completion notifier */

		printf("echo-server: ES_ACCEPTED\n");

		ret_err = ERR_OK;
	}
	else if (es->state == ES_RECEIVED)
	{
		// JSIM's variable
		int i;	
		int d;
		unsigned long array[2];
		struct msgbuf mbuf;
		int ret = -1;
		long *vmem = NULL;
		int proto_id;
		long target_addr;
		long offset;
		static u64 addr;

		/* JSIM
		 * 20170322 22:33
		 * count recv packet
		 * First two packets are CPU states, save it to 0x49005000 (physical addr)
		 * Use vmem_for_cpu (virtual memory for 0x49005000
		 */

		// original code
		/* read some more data */
		if(es->p == NULL)
		{
			es->p = p;
			// JSIM comment
			/*tcp_sent(tpcb, echo_sent);
			  echo_send(tpcb, es);*/
			/*if (aft_mig_in_mmio == 1)
				printf ("es->p == NULL\n");*/	// es->p == NULL true
		}
		else
		{
			struct pbuf *ptr;

			/* chain pbufs to the end of what we recv'ed previously  */
			ptr = es->p;
			pbuf_chain(ptr,p);
			/*if (aft_mig_in_mmio == 1)
				printf ("es->p is not null\n");*/
		}	// original ES_RECEIVED code end
#ifdef CONFIG_REDIS_DEBUG
		if (aft_mig_in_mmio == 0) {
			printf ("(%s) is still called!\n", __func__);
		}
#endif
		
		memcpy (&recv_msg, es->p->payload, SIZE_OF_METADATA);
		proto_id = bf_msg_get_proto_id (&recv_msg);
		/*memcpy (&msg, es->p->payload, SIZE_OF_METADATA);
		proto_id = bf_msg_get_proto_id (&msg);*/

		if(proto_id == PROTO_ID_META_DATA_CPU) {
			//get_acpi_time (&t_start[2]);
			printd_t ("echo-server: CPU states received \n");
			//printf("echo-server: vmem_for_cpu: %lx\n", vmem_for_cpu);
			//printf("echo-server: es->p->payload: %lx\n", es->p->payload);
			memcpy(vmem_for_cpu, es->p->payload + 24, SIZE_SENDBUF_DATA);
			
		} else {
			// requested address recv?	// it only contains last packet of requested addr
			//else if (proto_id == PROTO_ID_META_DATA_REQ_MEM) {*
			if (proto_id == PROTO_ID_META_DATA_REQ_MEM) {
				static int j = 0;
				// get_acpi_time 2018-11-12
				/*if (aft_mig_in_mmio == 1) {
					//get_acpi_time (&t_req[j++][1]);
				}*/
				printd ("(%s) proto id = %d\n", __func__, proto_id);
				DBGPC ("(%s) requested address recved, addr_to_migrate: %lx\n", __func__, recv_msg.meta.data_mem.addr_to_migrate);
				// select memory area
				set_mem_area_num (recv_msg.meta.data_mem.addr_to_migrate);	// this func set mem_area_num
				// save it to memory
				printd ("(%s) set offset\n", __func__);
				offset = (recv_msg.meta.data_mem.addr_to_migrate - usable_mem_list[mem_area_num].start_addr
						+ (recv_msg.meta.data_mem.batch_num * SIZE_SENDBUF_DATA)) >> 3;
				printd ("(%s) data copy\n", __func__);
				memcpy (usable_vmem_list[mem_area_num] + offset, es->p->payload + 24, SIZE_SENDBUF_DATA);
				// notify to ept_violation func
				ept_recved = true;
				// set_bitarray
				printd ("(%s) set bitarray\n", __func__);
				set_bitarray((u64)recv_msg.meta.data_mem.addr_to_migrate);	// FIXME It maybe desn't need to do 
#ifdef CONFIG_COUNT_BACKGROUND_PRESSURE
				request_is_sent = false;
				sum_back_receive += num_back_receive;
				num_back_receive = 0;
				request_recved_t = get_cpu_time();
				sum_request_recv_t += request_recved_t - request_sent_t;
#endif
				/*
				if (j > EXPR_NUM-1) {
					print_root_cause_overhead ();
				}*/	// FIXME
				// other recv?
				// 19-04-08 for full bandwidth
				//reception = false;
			} else if (proto_id == PROTO_ID_META_DATA_MEM) {
				int t_temp1;
				//get_acpi_time(&t_temp1);
				//printf ("single packet duration: %d\n", t3-t2);
				printd ("(%s) proto id = %d\n", __func__, proto_id);
				// get_bitarray
				//printf ("(%s) normal recv, addr_to_migrate: %lx\n", __func__, recv_msg.meta.data_mem.addr_to_migrate);
				//printf ("addr: %lx, batch num: %d\n", recv_msg.meta.data_mem.addr_to_migrate, recv_msg.meta.data_mem.batch_num);
				//printf ("batch size: %d\n", recv_msg.meta.data_mem.batch_size);
				//if (get_bitarray (recv_msg.meta.data_mem.addr_to_migrate) == 1 && recv_msg.meta.data_mem.addr_to_migrate > 0x100000) {
#ifdef CONFIG_POSTCOPY_DBG
				if (get_bitarray (recv_msg.meta.data_mem.addr_to_migrate) == 1) {
					// if result of get_bitarray == 1, print error. TODO erase get_bitarray if there's no problem
					printf ("error: get_bitarray returns 1, address: %lx\n", recv_msg.meta.data_mem.addr_to_migrate);
				}
#endif
				// select memory area
				printd ("(%s) select memory area\n", __func__);
				set_mem_area_num (recv_msg.meta.data_mem.addr_to_migrate);	// this func set mem_area_num
				printd ("(%s) selected memory area: %d\n", __func__, mem_area_num);
				// save it to memory
				offset = (recv_msg.meta.data_mem.addr_to_migrate - usable_mem_list[mem_area_num].start_addr
					+ (recv_msg.meta.data_mem.batch_num * SIZE_SENDBUF_DATA)) >> 3;

				printd ("(%s) data copy\n", __func__);
				memcpy (usable_vmem_list[mem_area_num] + offset, es->p->payload + 24, 
						SIZE_SENDBUF_DATA);
				printd ("(%s) usable_vmem_list[mem_area_num]: %lx \nusable_vmem_list[mem_area_num] + offset: %lx \noffset: %lx\n", __func__, usable_vmem_list[mem_area_num], usable_vmem_list[mem_area_num] + offset, offset);
				// if batch size == batch num
				if ((recv_msg.meta.data_mem.batch_size - 1) == recv_msg.meta.data_mem.batch_num) {
				//if ((recv_msg.meta.data_mem.batch_size) == recv_msg.meta.data_mem.batch_num + 1) {
					//int t_temp2;
					static int k = 0;
					// set_bitarray
					//printf ("(%s) recved a batch, address: %lx\n", __func__, recv_msg.meta.data_mem.addr_to_migrate);
					set_bitarray((u64)recv_msg.meta.data_mem.addr_to_migrate);
					DBGPC ("seq receive %lx\n", (u64)recv_msg.meta.data_mem.addr_to_migrate);
#ifdef CONFIG_COUNT_BACKGROUND_PRESSURE
					if (request_is_sent) {
						num_back_receive++;
					}
					num_recved_batch++;
					/*
					if (num_recved_batch % 20000 == 0) {
						if (num_request_sent != 0) {
							printf ("(%s) %d\n", __func__, 
								sum_back_receive / num_request_sent);
							printf ("(%s) sum_back_receive: %lld\n", __func__, 
									sum_back_receive);
							printf ("(%s) num_request_sent: %lld\n", __func__,
									num_request_sent);
							printf ("(%s) \n", __func__);
							printf ("(%s) request reply latency: %lld\n", __func__,
								sum_request_recv_t / num_request_sent);
							//printf ("(%s) \n", __func__);
						}
					}
					*/
#endif
					//get_acpi_time (&t_temp2);
					//t_margin[k++] = t_temp2 - t_temp1;
				}
			} /*else {
				printd ("(%s) proto id = %d\n", __func__, proto_id);
			}*/
			else if (proto_id == PROTO_ID_META_BUFF_MEM) {
				//get_acpi_time (&t_start[3]);
				set_mem_area_num (recv_msg.meta.data_mem.addr_to_migrate);	// this func set mem_area_num
				offset = (recv_msg.meta.data_mem.addr_to_migrate - usable_mem_list[mem_area_num].start_addr
						+ (recv_msg.meta.data_mem.batch_num * SIZE_SENDBUF_DATA)) >> 3;
				memcpy (usable_vmem_list[mem_area_num] + offset, es->p->payload + 24, SIZE_SENDBUF_DATA);

				if ((recv_msg.meta.data_mem.batch_size - 1) == recv_msg.meta.data_mem.batch_num) {
					u64 virtio_queue64 = (virtio_send_queue << 12) & 0xfffffffffff00000;
					// set_bitarray
					printd ("(%s) recved a batch\n", __func__);
					set_bitarray((u64)recv_msg.meta.data_mem.addr_to_migrate);	// virtio queue contents set_bitarray
					// set bitarray for margins
					/*set_multiple_bitarray(0x0, 0x9ffff);
					set_multiple_bitarray(0xe0000, 0xfffff);
					set_multiple_bitarray(0x86b35000, 0x86bfffff);
					set_multiple_bitarray(0x87900000, 0x879bffff);
					set_multiple_bitarray(0x87fcd000, 0x87ffffff);
					set_multiple_bitarray(virtio_queue64, virtio_queue64 + 0xfffff);*/
					//set_bitarray(0xe0000);	// not included in 128KB MU
					set_bitarray(0x86b35000);
					//set_bitarray(0x87900000);	// not included in 128KB MU
					set_bitarray(0x87fcd000);
				}

				printf ("recved BUFF_MEM\n");
				// 19-04-08 for full bandwidth
				//reception = false;

				// In case of post-copy migration, activate it.
				d = msgopen("postcopy_ready");
				if(d < 0) {
					printf("postcopy_ready not found.\n");
					return -1;
				}
				array[0] = 1;	// cpu migration done
				array[1] = vmem_for_cpu;
				setmsgbuf(&mbuf, array, sizeof array, 0);
				ret = msgsendbuf(d, 0, &mbuf, 1);
				msgclose(d);
				if(ret) {
					printf("postcopy migration msgsendbuf failed\n");
					return -1;
				}
			} else if (proto_id == PROTO_ID_META_DATA_MEM_SP) {
				/*if (get_bitarray (recv_msg.meta.data_mem.addr_to_migrate) == 1) {
					printf ("error: get_bitarray returns 1, address: %lx\n", recv_msg.meta.data_mem.addr_to_migrate);
				}*/
				set_mem_area_num (recv_msg.meta.data_mem.addr_to_migrate);	// this func set mem_area_num
				offset = (recv_msg.meta.data_mem.addr_to_migrate - usable_mem_list[mem_area_num].start_addr
					+ (recv_msg.meta.data_mem.batch_num * SIZE_SENDBUF_DATA)) >> 3;
				memcpy (usable_vmem_list[mem_area_num] + offset, es->p->payload + 24, 762);
				printf ("762-byte recved addr: %lx\n", recv_msg.meta.data_mem.addr_to_migrate);
				// You have to set bitarray here!
				set_bitarray((u64)recv_msg.meta.data_mem.addr_to_migrate);
			} else if (proto_id == PROTO_ID_TEST_SEND) {
				static int first = 1;
				if (first) {
					first = 0;
					addr = alloc (SIZE_SENDBUF_DATA);	// 1024
				}
				memcpy (addr, es->p->payload + 24, SIZE_SENDBUF_DATA);
				/*printf ("received batch: %d/%d\n", recv_msg.meta.data_mem.batch_num, 
									recv_msg.meta.data_mem.batch_size);*/
				// mimic DATA_MEM
				// Let's save packet to somewhere using memcpy
				// You can use same space to save it.
			} else if (proto_id == PROTO_ID_TEST_SEND_END) {
				// mimic DATA_REQ_MEM
				memcpy (addr, es->p->payload + 24, SIZE_SENDBUF_DATA);
				printf ("TEST_SEND_END\n");
				//printf ("TEST_SEND_END, received batch: %d/%d\n", recv_msg.meta.data_mem.batch_num, 
				//					recv_msg.meta.data_mem.batch_size);
			} else if (proto_id == PROTO_ID_META_MIG_DONE) {
				printf ("migration done, t_area: %d\n", t_area);
				aft_mig_in_mmio = 0;
			}
			// if all memory recved, aft_mig = 0;
			/*if (recv_msg.meta.data_mem.addr_to_migrate == 0xffffffffffffffff) {
				printf ("all memory recved\n");

				//aft_mig_in_mmio = 0;
			}*/
			//tcp_recved(tpcb, SIZE_OF_SENDBUF);	// commented 190318
		}

		/*if (get_bitarray(requested_addr)) {	// solve deadlock
			ept_recved = true;
		}*/

		//printf ("(%s) Received len:%d\n", __func__, p->tot_len);

		//tcp_recved(tpcb, p->tot_len);
		//u8_t freed;

		es->p = NULL;
		/*do
		{
			freed = pbuf_free(p);
		}
		while(freed == 0);*/
		pbuf_free(p);
		tcp_recved(tpcb, SIZE_OF_SENDBUF);
		ret_err = ERR_OK;
	}
	else if(es->state == ES_CLOSING)
	{
		printf("echo-server.c: ES_CLOSING\n");
		/* odd case, remote side closing twice, trash data */
		tcp_recved(tpcb, p->tot_len);
		es->p = NULL;
		pbuf_free(p);
		ret_err = ERR_OK;
	}
	else
	{
		printf("echo-server.c: ELSE unknown state\n");
		/* unkown es->state, trash data  */
		tcp_recved(tpcb, p->tot_len);
		es->p = NULL;
		pbuf_free(p);
		ret_err = ERR_OK;
	}
	return ret_err;
}

	static void
echo_error(void *arg, err_t err)
{
	struct echo_state *es;

	LWIP_UNUSED_ARG(err);

	es = (struct echo_state *)arg;
	if (es != NULL)
	{
		mem_free(es);
	}
}

	static err_t
echo_poll(void *arg, struct tcp_pcb *tpcb)
{
	err_t ret_err;
	struct echo_state *es;

	es = (struct echo_state *)arg;
	if (es != NULL)
	{
		if (es->p != NULL)
		{
			/* there is a remaining pbuf (chain)  */
			printf ("(%s) there is a remaining pbuf\n", __func__);
			tcp_sent(tpcb, echo_sent);
			echo_send(tpcb, es);
		}
		else
		{
			/* no remaining pbuf (chain)  */
			if(es->state == ES_CLOSING)
			{
				echo_close(tpcb, es);
			}
		}
		ret_err = ERR_OK;
	}
	else
	{
		/* nothing to be done */
		tcp_abort(tpcb);
		ret_err = ERR_ABRT;
	}
	return ret_err;
}

	static err_t
echo_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	struct echo_state *es;

	printf("echo-server.c: echo_sent\n");
	LWIP_UNUSED_ARG(len);

	es = (struct echo_state *)arg;
	es->retries = 0;

	if(es->p != NULL)
	{
		/* still got pbufs to send */
		tcp_sent(tpcb, echo_sent);
		echo_send(tpcb, es);
	}
	else
	{
		/* no more pbufs to send */
		if(es->state == ES_CLOSING)
		{
			echo_close(tpcb, es);
		}
	}
	return ERR_OK;
}

	static void
echo_send(struct tcp_pcb *tpcb, struct echo_state *es)
{
	struct pbuf *ptr;
	err_t wr_err = ERR_OK;

	printf("echo-server.c: echo_send\n");
	while ((wr_err == ERR_OK) &&
			(es->p != NULL) && 
			(es->p->len <= tcp_sndbuf(tpcb)))
	{
		ptr = es->p;

		/* enqueue data for transmission */
		wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);
		if (wr_err == ERR_OK)
		{
			u16_t plen;
			u8_t freed;

			plen = ptr->len;
			/* continue with next pbuf in chain (if any) */
			es->p = ptr->next;
			if(es->p != NULL)
			{
				/* new reference! */
				pbuf_ref(es->p);
			}
			/* chop first pbuf from chain */
			do
			{
				/* try hard to free pbuf */
				freed = pbuf_free(ptr);
			}
			while(freed == 0);
			/* we can read more data now */
			tcp_recved(tpcb, plen);
		}
		else if(wr_err == ERR_MEM)
		{
			/* we are low on memory, try later / harder, defer to poll */
			es->p = ptr;
		}
		else
		{
			/* other problem ?? */
		}
	}
}

static err_t
echo_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	//int ret_tcp_send = -1;

	return ERR_OK;
}

static err_t
request_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	//int ret_tcp_send = -1;
	//rt2 = get_cpu_time();
	//printf("request ack time: %d\n", rt2 - rt1);

	return ERR_OK;
}

static int
tcp_sendbuf_available(struct tcp_pcb *pcb)
{
	int available_space = 0;
	available_space = tcp_sndbuf(pcb) - SIZE_OF_SENDBUF;
	return available_space >= 0;
}

static int
tcp_send_buf (struct tcp_pcb *pcb)
{
	err_t err;

	while(!tcp_sendbuf_available(pcb)) {
		// Is it required?
	}	// JSIM 2017-07-14	// IF commented it, page-fault exception occured

	err = tcp_write (pcb, send_buf, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_MORE);

	if (err != ERR_OK) {
		printf("tcp_write failed. error_code:%d\n", err);

		return -1;
	}
	return err;
}

static int
tcp_send_msg (struct tcp_pcb *pcb)
{
	err_t err;

	/*while(!tcp_sendbuf_available(pcb)) {
		// Is it required?
	}*/	// JSIM 2017-07-14	// IF commented it, page-fault exception occured

	//err = tcp_write (pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);
	err = tcp_write (pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_COPY);
	tcp_output(pcb);

	if (err != ERR_OK) {
		printf("tcp_write failed. error_code:%d\n", err);
		return -1;
	}
	return err;
}

/* send a message to client */
/* not used now 2018-11-09 */
int
echo_server_send (unsigned long req_addr)
{
	//int ret_tcp_send = -1;

	//printf ("(%s) request from ept_violation\n", __func__);
	bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_REQ_MEM);
	msg.meta.data_mem.addr_to_migrate = req_addr;

	tcp_sent(echo_server_pcb, request_sent);

	//get_acpi_time(&rt1); FIXME
	//rt1 = get_cpu_time();
	tcp_write (echo_server_pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_COPY);
	tcp_output(echo_server_pcb);

	//ret_tcp_send = tcp_send_msg(echo_server_pcb);
	/*if(ret_tcp_send != ERR_OK) {
		printf("tcp_send_buf failed. error code:%d\n", ret_tcp_send);
		return -1;
	}*/

	return 0;
}

	static void
echo_close(struct tcp_pcb *tpcb, struct echo_state *es)
{
	tcp_arg(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_recv(tpcb, NULL);
	tcp_err(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);

	if (es != NULL)
	{
		mem_free(es);
	}  
	tcp_close(tpcb);
}

#endif /* LWIP_TCP */
