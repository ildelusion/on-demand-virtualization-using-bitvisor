/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
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
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

#if LWIP_TCP

#define NUM_OF_MEM_AREA 6
#define SIZE_OF_SENDBUF 512

static struct tcp_pcb *echo_pcb;
// JSIM's counter
static long recv_count_for_cpu;
// JSIM's vmem_for_cpu
static long* vmem_for_cpu;
// JSIM's memory list
struct usable_mem {
	long start_addr;
	long end_addr;
};

static struct usable_mem usable_mem_list[] = {
	{0x0, 0x9c600},
	{0x100000, 0x7e7fffff},
	{0x86b47000, 0x86b9afff},
	{0x86fbf000, 0x86fd3fff},
	{0x87fcd000, 0x87ffffff},
	{0x100000000, 0x2767fffff},
};

static long num_of_recv[NUM_OF_MEM_AREA];
static long num_of_recved[NUM_OF_MEM_AREA];
static long* usable_vmem_list[NUM_OF_MEM_AREA];
static int mem_area_num;
static long total_num_of_recv;
static long total_num_of_recved;

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
				num_of_recv[i] = mem_len/SIZE_OF_SENDBUF;	// count have to be received
				num_of_recved[i] = 0;
			}
			mem_area_num = 0;
			total_num_of_recved = 0;
			total_num_of_recv = 0;
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
	}
	else
	{
		ret_err = ERR_MEM;
	}
	return ret_err;  
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
		/* remote host closed connection */
		es->state = ES_CLOSING;
		if(es->p == NULL)
		{
			/* we're done sending, close it */
			echo_close(tpcb, es);
		}
		else
		{
			/* we're not done yet */
			tcp_sent(tpcb, echo_sent);
			echo_send(tpcb, es);
		}
		ret_err = ERR_OK;
	}
	else if(err != ERR_OK)
	{
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
		int i;	
		int d;
		unsigned long array[2];
		struct msgbuf mbuf;
		int ret = -1;
		long *vmem = NULL;
		/* first data chunk in p->payload */
		es->state = ES_RECEIVED;
		/* store reference to incoming pbuf (chain) */
		es->p = p;
		/* install send completion notifier */


		/* JSIM
		 * 20170322 22:33
		 * count recv packet
		 * First two packets are CPU states, save it to 0x49005000 (physical addr)
		 * Use vmem_for_cpu (virtual memory for 0x49005000
		 */
		if(recv_count_for_cpu < 2) {
			memcpy(vmem_for_cpu + recv_count_for_cpu*64, es->p->payload, SIZE_OF_SENDBUF);
			recv_count_for_cpu++;
		} else {
			// save received memory states to main memory
			// increase num_of_recved, is it reach to num_of_recv
			memcpy(usable_vmem_list[mem_area_num] + num_of_recved[mem_area_num], es->p->payload, SIZE_OF_SENDBUF);
			num_of_recved[mem_area_num] += SIZE_OF_SENDBUF/8;
			if(num_of_recved[mem_area_num] > num_of_recv[mem_area_num]*SIZE_OF_SENDBUF/8) {
				mem_area_num++;
			}
			total_num_of_recved++;
		}
		if(total_num_of_recved >= total_num_of_recv) {	// when memocy and cpu migration done
			d = msgopen("migration_ready");
			if(d < 0) {
				printf("migration not found.\n");
				return -1;
			}
			array[0] = 1;	// memory and cpu migration done
			setmsgbuf(&mbuf, array, sizeof array, 0);
			ret = msgsendbuf(d, 0, &mbuf, 1);
			msgclose(d);
			if(ret < 0) {
				printf("migration msgsendbuf failed\n");
				return -1;
			}
		}

		/* JSIM's code start */	// test code for 512-byte transfer

		/*
		vmem = (long*)mapmem(MAPMEM_HPHYS | MAPMEM_WRITE, 0x50000000, SIZE_OF_SENDBUF);

		for(i = 0; i < 64; i++) {
			if(i%6 == 0) printf("\n%d: \n", i);
			printf("%lx ", *(vmem + i));
		}

		memcpy(vmem, es->p->payload, SIZE_OF_SENDBUF);
		for(i=0; i < 64; i++) {
			if(i%6 == 0) printf("\n%d: \n", i);
			printf("%lx ", ((long*)(es->p->payload))[i]);
		}
		printf("\nbetween value\n");

		for(i = 0; i < 64; i++) {
			if(i%6 == 0) printf("\n%d: \n", i);
			printf("%lx ", *(vmem + i));
		}
		*/
		/* JSIM's code end */

		tcp_sent(tpcb, echo_sent);
		echo_send(tpcb, es);
		ret_err = ERR_OK;
	}
	else if (es->state == ES_RECEIVED)
	{
		/* read some more data */
		if(es->p == NULL)
		{
			es->p = p;
			tcp_sent(tpcb, echo_sent);
			echo_send(tpcb, es);
		}
		else
		{
			struct pbuf *ptr;

			/* chain pbufs to the end of what we recv'ed previously  */
			ptr = es->p;
			pbuf_chain(ptr,p);
		}
		ret_err = ERR_OK;
	}
	else if(es->state == ES_CLOSING)
	{
		/* odd case, remote side closing twice, trash data */
		tcp_recved(tpcb, p->tot_len);
		es->p = NULL;
		pbuf_free(p);
		ret_err = ERR_OK;
	}
	else
	{
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
