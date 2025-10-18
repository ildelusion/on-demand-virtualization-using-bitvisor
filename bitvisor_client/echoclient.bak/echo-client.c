#include <core/mm.h>		// to mapmem
#include <core/mmio.h>
#include <core/string.h>	// to memcpy
#include <core/process.h>
#include <core/initfunc.h>
#include <core/thread.h>	// New thread (postcopy migration)
#include <core/spinlock.h>	// locking for postcopy migration
#include <core/time.h>
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "biform-net-protocol/msg_protocol.h"
#include "echo-client.h"
//#include "echo.h"

static ip_addr_t destip;
static int destport;
static struct tcp_pcb *echo_client_pcb;

#if 0				/* debug? */
#define printd(X...) do { printf (X); } while (0)
#else
#define printd(X...)
#endif

#define TCP_SND_BUFFER 8192
#define NUM_OF_MEM_AREA 6
#define SEND_SIZE 1024
#define SIZE_OF_SENDBUF SEND_SIZE + 24	// 1024(data) + 24(meta)
// SIZE_SENDBUF_DATA 1024 in msg_protoco.h

//#define BITARRAY 4096
//#define SHIFT 20	// 2^20 = 1048576
//#define MIG_UNIT 14

/* 1MB UNIT MIGRATION */
#define BITARRAY 64	// 1 << (32 - SHIFT)
#define SHIFT 26	// MIG_UNIT + 6
#define MIG_UNIT 20	// 2^20 = 1M -> 1MB UNIT MIGRATION

#define BATCH_SIZE 1024

//#define DEVISOR 262144	// 4KB * 8 * 8

static char send_buf[SIZE_OF_SENDBUF];
static char* init_send_buf = "Hello, BitVisor!\n";
static int init_message;
static bf_msg_t msg;
static u64 counter;

static bool requested;
u64 requested_addr;
u64 requested_offset;
unsigned int req_mem_area_num;

u64 bitarray[BITARRAY];
u64 seq_idx;
u64 virtio_queue64;

static u64* usable_vmem_list[NUM_OF_MEM_AREA]; 
struct usable_mem {
	u64 start_addr;
	u64 end_addr;
};
static struct usable_mem usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	//{0x100000000, 0x2767fffff},
};

static long num_of_sent[NUM_OF_MEM_AREA];
static long num_of_send[NUM_OF_MEM_AREA];
static long num_of_send_point[NUM_OF_MEM_AREA];
static int mem_area_num;

static spinlock_t postcopy_lock;

struct echo_state
{
	u8_t state;
	u8_t retries;
	struct tcp_pcb *pcb;
	struct pbuf *p;
};

u64 t3, t4;
u64 st1, st2;

enum margin_type {
	MARGIN00,
	MARGIN01,
	MARGIN02,
	MARGIN03,
	MARGIN04,
	MARGIN_BUFF,
	//MEMORY_AREA00,
	MEMORY_AREA01,
	MEMORY_AREA02,
	MEMORY_AREA03,
	//MEMORY_AREA04,
	MEMORY_AREA05
};

struct copy_state
{
	u64 start_addr;
	u64 end_addr;
	short mem_area;
	unsigned int send_size;
	unsigned int send_num;
	enum margin_type m_type;
	unsigned int bitarray_num;
};

struct copy_state postcopy_state;

static err_t
postcopy_requested (void * arg, struct tcp_pcb *tpcb, u16_t len);
static err_t
postcopy_migration_callback (void * arg, struct tcp_pcb *tpcb, u16_t len);
static err_t
send_virtio_recv_buffer ();
static err_t
send_margins (void * arg, struct tcp_pcb *tpcb, u16_t len);

// check whether gphys is migrated or not
unsigned int get_bitarray(u64 gphys)
{
	u64 pos = gphys >> SHIFT;
	//printf("pos: %d\n", pos);
	return (bitarray[pos] >> ((gphys >> MIG_UNIT) & 0x3f)) & 1;
}

void set_bitarray(u64 gphys)
{
	unsigned int pos = gphys >> SHIFT;
	bitarray[pos] |= (1LL << ((gphys >> MIG_UNIT) & 0x3f));		// LL means 64-bit constant
}

/*
 * Set tcp send buf with memory data.
 */
void set_send_buf_for_mem()
{
	memcpy(send_buf, usable_vmem_list[mem_area_num] + num_of_sent[mem_area_num], SIZE_OF_SENDBUF);
	num_of_sent[mem_area_num] += SIZE_OF_SENDBUF/8;	// 8 = size of long pointer
	if(num_of_sent[mem_area_num] > num_of_send_point[mem_area_num]){
		printd ("(%s) num_of_send:%ld num_of_sent:%ld\n", __func__, 
				num_of_send[mem_area_num]*SIZE_OF_SENDBUF/8,
				num_of_sent[mem_area_num]);
		mem_area_num++;	// next mem area.
	}
}

//static unsigned long* cpu_state_vaddr = NULL;

/* If argument mem == NULL, it is the second request.*/
void set_send_buf_for_cpu(unsigned long* mem)
{
	memcpy(send_buf, mem, SIZE_SENDBUF_DATA);
}

void set_send_msg_for_cpu(unsigned long* mem)
{
	memcpy(msg.data, mem, SIZE_SENDBUF_DATA);
}

/**
  * Check whether tcp sendbuf is available.
  */
static int
tcp_sendbuf_available(struct tcp_pcb *pcb)
{
	//struct tcp_pcb *pcb = echo_client_pcb;
	int avail_space = 0;
	avail_space = tcp_sndbuf (pcb) - SIZE_OF_SENDBUF;
	//printd("(%s) avail_space:%d\n", __func__, avail_space);
	return avail_space >= 0;
}

/**
  * Do tcp_write.
  * Availability of tcp sendbuf should be checked
  * before this function is called.
  */
static int
tcp_send_buf (struct tcp_pcb *pcb)
{
	err_t err;
	err = tcp_write (pcb, send_buf, SIZE_SENDBUF_DATA,
		TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);	// No need to copy. send_buf is modified after ACK from host. JYKim.
		//	TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		printd ("(%s) tcp_write failed. error_code:%d\n", __func__,
				err);
		return -1;
	}
	return err;
}

static int
tcp_send_msg (struct tcp_pcb *pcb)
{
	err_t err;

	/*err = tcp_write (pcb, &msg, SIZE_OF_SENDBUF,
			TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);*/	// No need to copy. send_buf is modified after ACK from host. JYKim.
	err = tcp_write (pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		printd ("(%s) tcp_write failed. error_code:%d\n", __func__,
				err);
		return -1;
	}
	return err;
}

static long int num_send = 0;
static long total_num_send = 0;

/**
  * It is called from echoctl.c
  */
int
echo_client_send (unsigned long* mem, long total_num_of_send)	// JSIM add argument. mem is virtual address which saves CPU states. Same virtual address in differenc processes? TODO JYKIM.
{
	//printf("echo_client_send called\n");
	struct tcp_pcb *pcb = echo_client_pcb;
	int ret_tcp_send = -1;
	int i = 0;

	// Set total num static variable.
	if (total_num_of_send != 0){
		total_num_send = total_num_of_send;
	}

	if (!echo_client_pcb) {
		printd ("(%s) No connection.\n", __func__);
		return -1;
	}

	// set counter, memcpy usable_vmem_list to send_buf by 512-byte. 1-kbyte unit make problem about sending garbage (I think) data for second(last) half.
	if(init_message == 0) {  // Initial check. It is printed when do "client send"
		printf("send H i to server\n");
		printd ("(%s) it is init_message.\n", __func__);
		send_buf[0] = 'H';
		send_buf[1] = 'i';
		send_buf[2] = '\n';
		send_buf[3] = '\0';

		ret_tcp_send = tcp_send_buf(pcb);	// send
		if (ret_tcp_send != ERR_OK){
			printd("(%s) tcp_send_buf failed. error code:%d\n", __func__, ret_tcp_send);
			return -1;
		};
		init_message++;

	
	}
	return 0;
}

static err_t
request_send (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	get_acpi_time(&st2);
	//printf("request send ack time: %d\n", st2 - st1);
	return 0;
}

static err_t
postcopy_requested_sent (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	spinlock_unlock (&postcopy_lock);
	return 0;
}

static err_t
postcopy_requested (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset;
	int ret_tcp_send = -1;

	//get_acpi_time(&st2);
	//printf("request send ack time: %d\n", st2 - st1);
	// send 1024 packets
	while (tcp_sendbuf_available(tpcb)) {
		printd ("(%s) set offset\n", __func__);
		offset = (requested_offset + batch * SIZE_SENDBUF_DATA) >> 3;		// SIZE_SENDBUF_DATA == 1024
		msg.meta.data_mem.batch_num = batch;
		printd ("(%s) offset: %lx\n", __func__, offset);
		memcpy (msg.data, usable_vmem_list[req_mem_area_num] + offset, SIZE_SENDBUF_DATA);
		printd ("(%s) usable_vmem_list[req_mem_area_num]: %lx \n usable_vmem_list + offset: %lx\n", __func__, usable_vmem_list[req_mem_area_num], usable_vmem_list[req_mem_area_num] + offset);
		batch++;
		printd ("(%s) batch: %d\n", __func__, batch);
		printd ("(%s) proto_id: %d\n", __func__, bf_msg_get_proto_id(&msg));
		if (batch >= BATCH_SIZE) {
			printf ("(%s) batch is larger than batch_size, batch: %d, requested_addr: %lx\n", __func__, batch, requested_addr);
			batch = 0;
			tcp_sent (tpcb, postcopy_requested_sent);

			bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_REQ_MEM);	// 1MB memory is sent
			ret_tcp_send = tcp_send_msg(tpcb);
			tcp_output(tpcb);
			/* if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			} */
			set_bitarray(requested_addr);
			//requested = false;
			return ERR_OK;	// batch end
		} else {
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, postcopy_requested);
			} else {
				tcp_sent(tpcb, NULL);
			}
			printd ("(%s) send batch: %d\n", __func__, batch);
			// wait
			//usleep(10000);	// wait 0.01 second
			//get_acpi_time(&st1);
			ret_tcp_send = tcp_send_msg(tpcb);
			// You don't need to use tcp_output(tpcb) here.
			/* if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
			 	return -1;
			} */
		}
	}
}

static err_t
margins_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	spinlock_unlock (&postcopy_lock);
	//thread_new (seq_transmit_start, NULL, VMM_STACKSIZE);
	return 0;
}

static err_t
send_margin (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned long offset;
	static unsigned int batch = 0;
	int ret_tcp_send = -1;
	int i;
	
	while (tcp_sendbuf_available(tpcb)) {
		offset = (unsigned long)(postcopy_state.start_addr - usable_mem_list[postcopy_state.mem_area].start_addr + postcopy_state.send_num * SIZE_SENDBUF_DATA) >> 3;
		memcpy (msg.data, usable_vmem_list[postcopy_state.mem_area] + offset, SIZE_SENDBUF_DATA);
		msg.meta.data_mem.batch_num = batch;
		postcopy_state.send_num++;
		batch++;

		if (postcopy_state.send_num >= postcopy_state.send_size) {
			switch (postcopy_state.m_type) {
				case MARGIN00:
					batch = 0;
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MARGIN01;
					break;
				case MARGIN01:
					batch = 0;
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MARGIN02;
					break;
				case MARGIN02:
					batch = 0;
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MARGIN03;
					break;
				case MARGIN03:
					batch = 0;
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MARGIN04;
					break;
				case MARGIN04:
					batch = 0;
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MEMORY_AREA01;
					break;
				case MARGIN_BUFF:
					set_bitarray (postcopy_state.start_addr);
					tcp_sent (tpcb, margins_sent);
					bf_msg_set_proto_id (&msg, PROTO_ID_META_BUFF_MEM);
					break;
				case MEMORY_AREA01:
					batch = 0;
					for (i = 0; i <= postcopy_state.bitarray_num; i++)
						set_bitarray (postcopy_state.start_addr + (i << MIG_UNIT));
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MEMORY_AREA02;
					break;
				case MEMORY_AREA02:
					batch = 0;
					for (i = 0; i <= postcopy_state.bitarray_num; i++)
						set_bitarray (postcopy_state.start_addr + (i << MIG_UNIT));
					tcp_sent (tpcb, send_margins);
					postcopy_state.m_type = MEMORY_AREA03;
					break;
				case MEMORY_AREA03:
					batch = 0;
					for (i = 0; i <= postcopy_state.bitarray_num; i++)
						set_bitarray (postcopy_state.start_addr + (i << MIG_UNIT));
					//tcp_sent (tpcb, send_margins);
					tcp_sent (tpcb, margins_sent);
					bf_msg_set_proto_id (&msg, PROTO_ID_META_BUFF_MEM);
					//postcopy_state.m_type = MEMORY_AREA05;
					break;
				case MEMORY_AREA05:	// eliminate
					batch = 0;
					for (i = 0; i <= postcopy_state.bitarray_num; i++)
						set_bitarray (postcopy_state.start_addr + (i << MIG_UNIT));
					tcp_sent (tpcb, margins_sent);
					bf_msg_set_proto_id (&msg, PROTO_ID_META_BUFF_MEM);
					//postcopy_state.m_type = MARGIN_BUFF;
					break;
				default:
					printf ("error! default never happen\n");
					break;
			}

			ret_tcp_send = tcp_send_msg (tpcb);
			tcp_output(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_buf failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}

			return ERR_OK;	// batch end
		} else {
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, send_margin);
			} else {
				tcp_sent(tpcb, NULL);
			}
			printd ("(%s) send batch\n", __func__);
			ret_tcp_send = tcp_send_msg(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}
		}

		if (batch == 1024) {
			batch = 0;
			msg.meta.data_mem.addr_to_migrate += 0x100000;
		}
	}
}

void set_send_margin (u64 start_addr, u64 end_addr)
{
	int i = 0;
	postcopy_state.start_addr = start_addr;
	postcopy_state.end_addr = end_addr;
	postcopy_state.send_size = (end_addr - start_addr + 1)/SEND_SIZE;
	
	for(i = 0; i < 6; i++) {
		if(start_addr >= usable_mem_list[i].start_addr && end_addr <= usable_mem_list[i].end_addr) {
			postcopy_state.mem_area = i;
			break;
		}
	}

	postcopy_state.send_num = 0;
	postcopy_state.bitarray_num = (end_addr - start_addr) >> MIG_UNIT;

	bf_msg_set_proto_id (&msg, PROTO_ID_META_DATA_MEM);
	msg.meta.data_mem.addr_to_migrate = start_addr;

	if (postcopy_state.m_type == MEMORY_AREA01 || 
		postcopy_state.m_type == MEMORY_AREA02 || 
		postcopy_state.m_type == MEMORY_AREA03 || 
		postcopy_state.m_type == MEMORY_AREA05)
		msg.meta.data_mem.batch_size = 1024;
	else 
		msg.meta.data_mem.batch_size = postcopy_state.send_size;

}

static err_t
send_margins (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	if (postcopy_state.m_type == MARGIN00) {
		printf ("MARGIN00 send\n");
		set_send_margin (0x0, 0x9ffff);
		spinlock_lock (&postcopy_lock);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN01) {
		printf ("MARGIN01 send\n");
		set_send_margin (0xe0000, 0xfffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN02) {
		printf ("MARGIN02 send\n");
		set_send_margin (0x86b35000, 0x86bfffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN03) {
		printf ("MARGIN03 send\n");
		set_send_margin (0x87900000, 0x879bffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN04) {
		printf ("MARGIN04 send\n");
		set_send_margin (0x87fcd000, 0x87ffffff);
		send_margin (NULL, echo_client_pcb, 0);

	} else if (postcopy_state.m_type == MEMORY_AREA01) {
		printf ("MEMORY_AREA01 send\n");
		set_send_margin (0x100000, 0x3fffffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MEMORY_AREA02) {
		printf ("MEMORY_AREA02 send\n");
		set_send_margin (0x60000000, 0x7e7fffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MEMORY_AREA03) {
		printf ("MEMORY_AREA03 send\n");
		set_send_margin (0x86c00000, 0x878fffff);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MEMORY_AREA05) {
		printf ("MEMORY_AREA05 send\n");
		set_send_margin (0xff000000, 0xffffffff);
		send_margin (NULL, echo_client_pcb, 0);
	} 
	/*
	else if (postcopy_state.m_type == MARGIN_BUFF) {	// virtio buffers
		if ((((u64)virtio_recv_queue << 12) & 0xfff00000) != (((u64)virtio_send_queue << 12) & 0xfff00000))
			printf ("Error: virtio_recv_queue and send_queue masking results are not equal!!!\n");
		else {
			virtio_queue64 = ((u64)virtio_send_queue << 12) & 0xfff00000;
			set_send_margin (virtio_queue64, virtio_queue64 + 0xfffff);
			send_margin (NULL, echo_client_pcb, 0);
		}
	}*/
}

static err_t
postcopy_sequential_thread (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset;
	int ret_tcp_send = -1;
	//int i;

	while (tcp_sendbuf_available(tpcb)) {
		// set sendbuf <- seq_idx
		if(seq_idx != 0xffffffffffffffff) {
			printd ("(%s) set offset\n", __func__);
			offset = (unsigned long)(seq_idx - usable_mem_list[mem_area_num].start_addr + batch * SIZE_SENDBUF_DATA) >> 3;	// /8 == >>3
			memcpy (msg.data, usable_vmem_list[mem_area_num] + offset, SIZE_SENDBUF_DATA);
			msg.meta.data_mem.batch_num = batch;
			batch++;
		}
		
		if (batch >= get_batch_size(seq_idx)) {
			// send last packet of this batch
			printd ("(%s) batch is larger than batch_size, batch: %d\n", __func__, batch);
			batch = 0;

			if (seq_idx == 0xffffffffffffffff) {
				// implement it! // FIXME
			}
			//FIXME
			
			
			
		} else {

			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent (tpcb, postcopy_sequential_thread);
			} else {
				tcp_sent (tpcb, NULL);
			}
			ret_tcp_send = tcp_send_msg (tpcb);

			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}
		}
	}
}

static err_t
postcopy_sequential (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset;
	int ret_tcp_send = -1;
	//int i;

	while (tcp_sendbuf_available(tpcb)) {
		// set sendbuf <- seq_idx
		if(seq_idx != 0xffffffffffffffff) {
			printd ("(%s) set offset\n", __func__);
			offset = (unsigned long)(seq_idx - usable_mem_list[mem_area_num].start_addr + batch * SIZE_SENDBUF_DATA) >> 3;	// /8 == >>3
			memcpy (msg.data, usable_vmem_list[mem_area_num] + offset, SIZE_SENDBUF_DATA);
			msg.meta.data_mem.batch_num = batch;
			batch++;
		}
		
		if (batch >= BATCH_SIZE) {
			// send last packet of this batch
			printd ("(%s) batch is larger than batch_size, batch: %d\n", __func__, batch);
			batch = 0;

			if (seq_idx == 0xffffffffffffffff)	// -1LL
			{
				printf ("(%s) migration done1\n", __func__);
				bf_msg_set_proto_id(&msg, PROTO_ID_META_MIG_DONE);
				msg.meta.data_mem.addr_to_migrate = seq_idx;
				tcp_sent(tpcb, NULL); // migration done

				ret_tcp_send = tcp_send_msg(tpcb);
				if (ret_tcp_send != ERR_OK) {
					printd("(%s) tcp_send_buf failed. error code:%d\n", __func__, ret_tcp_send);
					return -1;
				}
				return ERR_OK;	// batch end
			}

			set_bitarray(seq_idx);
			seq_idx = get_next_seq_idx(seq_idx);

			if (seq_idx == 0xffffffffffffffff)
			{ 
				printf ("(%s) migration done\n", __func__);
				batch = 16;
				tcp_sent(tpcb, postcopy_sequential); // migration done
			} else {
				tcp_sent (tpcb, postcopy_migration_callback);
			}

			ret_tcp_send = tcp_send_msg(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_buf failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}
			return ERR_OK;	// batch end
		} else {
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, postcopy_sequential);
			} else {
				tcp_sent(tpcb, NULL);
			}
			printd ("(%s) send batch\n", __func__);
			ret_tcp_send = tcp_send_msg(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}
		}
	}
}

u64 get_next_seq_idx(u64 prev_seq_idx)
{
	u64 next_seq_idx;

	if (prev_seq_idx == usable_mem_list[3].start_addr) {
		return 0x86b38000;
	} else if (prev_seq_idx == usable_mem_list[4].start_addr) {
		return 0x87fd0000;
	}
	// get next seq_idx
	next_seq_idx = (prev_seq_idx + 0x4000) & 0xffffffffffffc000;
	
	// calculate mem_area_num
	if (next_seq_idx == ((usable_mem_list[0].end_addr + 0x4000) & 0xffffffffffffc000)) {
		next_seq_idx = usable_mem_list[1].start_addr;
		mem_area_num++;
	} else if (next_seq_idx == ((usable_mem_list[1].end_addr + 0x4000) & 0xffffffffffffc000)) {
		next_seq_idx = usable_mem_list[2].start_addr;
		mem_area_num++;
	} else if (next_seq_idx == ((usable_mem_list[2].end_addr + 0x4000) & 0xffffffffffffc000)) {
		next_seq_idx = usable_mem_list[3].start_addr;
		mem_area_num++;
	} else if (next_seq_idx == ((usable_mem_list[3].end_addr + 0x4000) & 0xffffffffffffc000)) {
		next_seq_idx = usable_mem_list[4].start_addr;
		mem_area_num++;
	} else if (next_seq_idx == ((usable_mem_list[4].end_addr + 0x4000) & 0xffffffffffffc000)) {
		next_seq_idx = usable_mem_list[5].start_addr;
		mem_area_num++;
	} else if (next_seq_idx == ((usable_mem_list[5].end_addr + 0x4000) & 0xffffffffffffc000)) {
		// if all memory sent, set next_seq_idx to -1
		printf("next_seq_idx: 0xffffffffffffffff\n");
		next_seq_idx = 0xffffffffffffffff;
	}
	
	printd ("(%s) next_seq_idx: %lx\n", __func__, next_seq_idx);
	return next_seq_idx;
}

int get_batch_size(u64 addr)
{
	//int batch_size = 0;
	
	// calculate possible batch size (1 ~ 16)
	if (addr >= usable_mem_list[3].start_addr && addr < 0x86b38000) {
		return 12;
	} else if (addr >= usable_mem_list[4].start_addr && addr < 0x87fd0000) {
		return 12;
	} else {
		return 16;
	}
}

/* select_mem_area func is used in postcopy_requested func
*/
void select_mem_area()
{
	if (requested_addr >= usable_mem_list[0].start_addr && requested_addr <= usable_mem_list[0].end_addr)
		req_mem_area_num = 0;
	else if (requested_addr >= usable_mem_list[1].start_addr && requested_addr <= usable_mem_list[1].end_addr)
		req_mem_area_num = 1;
	else if (requested_addr >= usable_mem_list[2].start_addr && requested_addr <= usable_mem_list[2].end_addr)
		req_mem_area_num = 2;
	else if (requested_addr >= usable_mem_list[3].start_addr && requested_addr <= usable_mem_list[3].end_addr)
		req_mem_area_num = 3;
	else if (requested_addr >= usable_mem_list[4].start_addr && requested_addr <= usable_mem_list[4].end_addr)
		req_mem_area_num = 4;
	else if (requested_addr >= usable_mem_list[5].start_addr && requested_addr <= usable_mem_list[5].end_addr)
		req_mem_area_num = 5;
	else
		printf("requested addr is out of range\n");

	requested_addr = requested_addr & 0xfffffffffff00000;
	requested_offset = requested_addr - usable_mem_list[req_mem_area_num].start_addr;

	/*if (requested_addr_temp >= usable_mem_list[3].start_addr && requested_addr_temp < 0x86b38000) {
		requested_addr = usable_mem_list[3].start_addr;
		requested_offset = 0;
	} else if (requested_addr_temp >= usable_mem_list[4].start_addr && requested_addr_temp < 0x87fd0000) {
		requested_addr = usable_mem_list[4].start_addr;
		requested_offset = 0;
	} else {
		requested_addr = (u64)requested_addr & 0xffffffffffffc000;
		requested_offset = (u64)requested_addr - usable_mem_list[req_mem_area_num].start_addr;
	}*/
}

static void
seq_transmit(void)
{
	static int sleep_time = 0;

	//if (sleep_time >= 100) {	// set it 100
	if (sleep_time >= 1) {	// set it 100
		// send seq memory
		sleep_time = 0;
		postcopy_sequential_thread (NULL, echo_client_pcb, 0);
	} else {
		usleep(100);	// sleep 100us
		sleep_time++;
	}
}

static void
seq_transmit_start (void *arg)
{
	for (;;) {
		seq_transmit();
		schedule ();
	}
}

static err_t
cpu_recv_callback (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	spinlock_unlock (&postcopy_lock);
	
	send_margins (NULL, echo_client_pcb, 0);

	return 0;
}

/* post-migration version send */
int
echo_client_send_postcopy_migration (unsigned long* mem, long total_num_of_send)
{
	struct tcp_pcb *pcb = echo_client_pcb;
	static bool cpu_state_sent = false;
	int ret_tcp_send = -1;

	if (!echo_client_pcb) {
		printd ("(%s) No connection.\n", __func__);
		return -1;
	}

	printd ("(%s) tcp_sent\n", __func__);
	//tcp_sent (pcb, postcopy_migration_callback); // set callback function.
	//tcp_sent (pcb, postcopy_kernel_memory_callback); // set callback function.
	tcp_sent (pcb, cpu_recv_callback); // set callback function.

	spinlock_lock (&postcopy_lock);
	bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_CPU);

	printd ("(%s) set_send_msg_for_cpu\n", __func__);
	set_send_msg_for_cpu (mem);

	printd ("(%s) tcp_send_msg\n", __func__);
	ret_tcp_send = tcp_send_msg(pcb);	// send	CPU state
	if (ret_tcp_send != ERR_OK){
		printd("(%s) tcp_send_msg for cpu state 1 failed. error code:%d\n", 
				__func__, ret_tcp_send);
		return -1;
	}
	return 0;
}

static err_t
echo_client_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	printd ("echo_client_sent: Sent.\n");
	return ERR_OK;
}

	static void
echo_client_error (void *arg, err_t err)
{
	printd ("Error: %d\n", err);
}

static err_t
request_recv (void)
{
	if(get_bitarray(requested_addr)) {
		return ERR_OK;	// already sent
	} else {	// should send
		select_mem_area();
		bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_MEM);
		printf ("(%s) requested_addr: %lx\n", __func__, requested_addr);
		msg.meta.data_mem.addr_to_migrate = requested_addr;
		msg.meta.data_mem.batch_size = BATCH_SIZE;	// 1024
		spinlock_lock (&postcopy_lock);
		get_acpi_time(&t4);
		//printf("recv ~ request_recv func: %d\n", t4-t3);
		postcopy_requested (NULL, echo_client_pcb, 0);

		return ERR_OK;
	}
}

	static err_t
echo_client_recv (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	int i;
	char *str;
	bf_msg_t *recv_msg;
	struct echo_state *es;

	LWIP_ASSERT("arg != NULL", arg != NULL);
	es = (struct echo_state *)arg;

	printd ("echo-client.c: Received.\n");
	if (!p) {
		/* Disconnected by remote. */
		printd ("Disconnected!\n");
		echo_client_pcb = NULL;
		return ERR_OK;
	} else if (err != ERR_OK) {
		/* Error occurred. */
		printd ("Error: %d\n", err);
		return err;
	} else {
		if(es->p == NULL)
		{
			es->p = p;
		} else 
		{
			struct pbuf *ptr;
			
			ptr = es->p;
			pbuf_chain(ptr, p);
		}
		// JSIM add recv_msg
		recv_msg = p->payload;
		if (bf_msg_get_proto_id(recv_msg) == PROTO_ID_META_DATA_REQ_MEM) {
			get_acpi_time(&t3);
			//printf("requested true\n");
			//requested = true;
			requested_addr = (u64)((*recv_msg).meta.data_mem.addr_to_migrate);
			printf("receive, requested_addr: %lx\n", requested_addr);
			request_recv ();
		}
		/* Really received. */
		tcp_recved (pcb, p->len);
		es->p = NULL;
		pbuf_free(p);	// 180518 appended

		/*for (i = 0; i < p->len; i++)
			printf ("%c", str[i]);*/
		return ERR_OK;
	}
}

static err_t
echo_client_connected (void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct echo_state *es;

	//LWIP_UNUSED_ARG(arg);
	//LIWP_UNUSED_ARG(err);
	
	es = (struct echo_state *)mem_malloc(sizeof(struct echo_state));

	if (err == ERR_OK) {
		if (es != NULL) {
		printf ("Connection established!\n");
		es->state = 0;
		es->pcb = pcb;
		es->retries = 0;
		es->p = NULL;

		tcp_arg (pcb, es);
		tcp_sent (pcb, echo_client_sent);
		tcp_recv (pcb, echo_client_recv);
		tcp_err (pcb, echo_client_error);
		} else {
			return ERR_MEM;
		}
	} else {
		printf ("Connection missed!\n");
	}
	return ERR_OK;
}

void
echo_client_init (int *ipaddr, int port)
{
	struct tcp_pcb *pcb;
	err_t e;
	int i = 0;
	long mem_start = 0;
	long mem_len = 0;
	int usable_mem_list_len = sizeof(usable_mem_list)/sizeof(usable_mem_list[0]);

	IP4_ADDR (&destip,
			ipaddr[0],
			ipaddr[1],
			ipaddr[2],
			ipaddr[3]);
	destport = port;

	printd ("New Connection.\n");
	pcb = tcp_new ();
	if (pcb) {
		tcp_nagle_disable(pcb);	// JYKIM added. for optimization.
		e = tcp_connect (pcb, &destip, destport,
				echo_client_connected);
		if (e == ERR_OK) {
			printf ("Connecting...\n");
			echo_client_pcb = pcb;
		} else {
			printf ("Connect failed!\n");
		}
	} else {
		printf ("New context failed.\n");
	}
	/* allocate memory to send to target machine */
	for(i=0; i < usable_mem_list_len; i++) {
		mem_start = usable_mem_list[i].start_addr;
		mem_len = usable_mem_list[i].end_addr - usable_mem_list[i].start_addr + 1;
		usable_vmem_list[i] = (long*)mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, mem_start, mem_len);
		printf ("JSIM: mapmem of %lx\n", mem_start);
		num_of_send[i] = mem_len/SIZE_SENDBUF_DATA;
		num_of_send_point[i] = mem_len/8;
		num_of_sent[i] = 0;
	}
	seq_idx = usable_mem_list[0].start_addr;
	bf_msg_init_msg(&msg);
	init_message = 0;
	memset(bitarray, 0, BITARRAY*8);	// 8-Byte * 4096

	printf("mig_done value at echo_client_init: %d\n", mig_done);
}
