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

#if 1				/* debug? */
#define printd_t(X...) do { printf (X); } while (0)
#else
#define printd_t(X...)
#endif

#define CONFIG_DBG_COUNTING
//#define CONFIG_EXT_INT_DBG

//#define POSTCOPY_DBG 1
#define POSTCOPY_DBG 0
#define DBGPC if (POSTCOPY_DBG) printf
// Use like this
//DBGPC ("(%s) \n", __func__);

#define POSTCOPY_DBG_2 1
//#define POSTCOPY_DBG_2 0
#define DBGPC_2 if (POSTCOPY_DBG_2) printf

#define CONFIG_POSTCOPY_MEASURE
#define POSTCOPY_MEASURE_PRINT 1
//#define POSTCOPY_MEASURE_PRINT 0
#define PRINT_MEASURE if (POSTCOPY_MEASURE_PRINT) printf
//PRINT_MEASURE ("(%s) \n", __func__);

#define RESEND_IN_THREAD 1
#define PRINT_DBG if (RESEND_IN_THREAD) printf
//PRINT_DBG ("(%s) \n", __func__);

//#define CONFIG_BACKGROUND_COPY_LIMIT
//#define CONFIG_DEBUG_GET_NEXT_ADDR
#define CONFIG_BITARRAY_LOCK
#define CONFIG_SENDBUF_DBG
#define CONFIG_RESEND_IN_THREAD

#define BACKGROUND_COPY
//#define BACKGROUND_COPY_DBG
#define GRAN_32KB
//#define CONFIG_MIG_DBG

#define TCP_SND_BUFFER 8192
//#define SEND_SIZE 1536	// You cannot use larger than MTU-40 (1500-40) == 1460
// Also you have 24-byte metadata here. So, you can use maximum 1436-byte here
// 40 means TCP header + IP header size (40-byte)
#define SEND_SIZE 1024
#define SIZE_OF_SENDBUF SEND_SIZE + 24	// 1024(data) + 24(meta)
// SIZE_SENDBUF_DATA 1024 in msg_protoco.h

//#define BITARRAY 4096
//#define SHIFT 20	// 2^20 = 1048576
//#define MIG_UNIT 14

//#define NUM_OF_MEM_AREA 6
#define NUM_OF_MEM_AREA 7
/* 1MB UNIT MIGRATION */
//#define BITARRAY 64	// 1 << (32 - SHIFT)	// 4GB
//#define BITARRAY 320	// 1 << (32 - SHIFT)	// 16GB, (256 + 64)

//Now (2022-03-15), you just need to change MIG_UNIT
#ifdef GRAN_32KB
#define MIG_UNIT 15	// 2^15 = 32KB
#define SHIFT (MIG_UNIT + 6)
#define BITARRAY ((1UL << (34 - MIG_UNIT - 6)) + 1024)	// 2^23 = 16GB, 2^6 = 64-bit array
// You have to cover up to 17.86GB
// For the safety, I cover up to +2GB (16 + 2GB)
#define ARRAY_MULTIPLIER (1UL << MIG_UNIT)
#define ADDR_OFFSET ARRAY_MULTIPLIER
#define MARGIN_OFFSET (ADDR_OFFSET - 1)
#define REQUEST_MASK (~MARGIN_OFFSET)

#else	// 128KB
#define BITARRAY 2400	// 1 << (32 - SHIFT)	// 16GB, 512 * 4 + a
//#define SHIFT 26	// MIG_UNIT + 6
#define SHIFT 23	// MIG_UNIT + 6
//#define MIG_UNIT 20	// 2^20 = 1M -> 1MB UNIT MIGRATION
#define MIG_UNIT 17	// 128KB UNIT MIGRATION
#define ARRAY_MULTIPLIER 0x20000	// 128KB 1 << 17
#define ADDR_OFFSET 0x20000		// 128KB	
#define MARGIN_OFFSET 0x1ffff		// 128KB
#define REQUEST_MASK 0xfffffffffffe0000	// 128KB, 1MB: 0xfffffffffff00000
#endif

//#define BATCH_SIZE 1024
//#define BATCH_SIZE 128	// when SEND_SIZE 1024	// to compare with 1.5KB, set this to 96
#define BATCH_SIZE 32	// when SEND_SIZE 1024	// to compare with 1.5KB, set this to 96
//#define BATCH_SIZE 96	// when SEND_SIZE 1024	// to compare with 1.5KB, set this to 96
//#define BATCH_SIZE 64	// when SEND_SIZE 1536 --> 1.5KB * 64 = 96KB
#define EXPR_NUM 300

//#define DEVISOR 262144	// 4KB * 8 * 8

static char send_buf[SIZE_OF_SENDBUF];
static char* init_send_buf = "Hello, BitVisor!\n";
static int init_message;
static bf_msg_t msg;
static bf_msg_t seq_msg;
static u64 counter;
u64 batch_size_param;

static bool requested;
u64 requested_addr;
u64 requested_offset;
unsigned int req_mem_area_num;

static bool migration_done;
//bool migration_done_msg_sent;

u64 bitarray[BITARRAY];
u64 seq_idx;
u64 virtio_queue64;
u64 postcopy_stat[20];

static u64* usable_vmem_list[NUM_OF_MEM_AREA]; 
struct mem_area {
	u64 start_addr;
	u64 end_addr;
};
/*static struct mem_area usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
};*/

static struct mem_area usable_mem_list[] = {
	{0x0000, 0x9c6f9},
	{0x100000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b47000, 0x86b9bfff},
	{0x86fbf000, 0x86fd4fff},
	{0x87fcd000, 0x87ffffff},
	{0x100000000, 0x4767fffff},
};

#ifdef GRAN_32KB
static struct mem_area usable_mem_list_gran[] = {
	{0x0000, 0x97fff},
	//{0x100000, 0x2fffff},
	{0x100000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b48000, 0x86b97fff},
	{0x86fc0000, 0x86fcffff},
	{0x87fd0000, 0x87ffffff},
	{0x100000000, 0x4767fffff},
};
#endif

static struct mem_area margin_list[] = {
	{0x98000, 0x9c3ff},
	{0x86b47000, 0x86b47fff},
	{0x86b98000, 0x86b9bfff},
	{0x86fbf000, 0x86fbffff},
	{0x86fd0000, 0x86fd4fff},
	{0x87fcd000, 0x87fcffff},
	{0x9c400, 0x9c6f9},
};

/* margin
 * 0x98000 - 0x9c6f9		17KB + 762B
 *	* 0x98000 - 0x9c3ff	17KB	(MARGIN00)
 *	* 0x9c400 - 0x9c6f9	762B	(MARGIN06SP)	// special
 * 0x86b47000 - 0x86b47fff	4KB	(MARGIN01)
 * 0x86b98000 - 0x86b9bfff	16KB	(MARGIN02)
 * 0x86fbf000 - 0x86fbffff	4KB	(MARGIN03)
 * 0x86fd0000 - 0x86fd4fff	20KB	(MARGIN04)
 * 0x87fcd000 - 0x87fcffff	12KB	(MARGIN05)
 */

/*
static struct mem_area usable_mem_list[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b35000, 0x879bffff},
	{0x87fcd000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	{0x100000000, 0x4767fffff},
};
*/

/*
// This is old one
static struct mem_area usable_mem_list_gran[] = {
	{0x0000, 0x9ffff},
	{0xe0000, 0x3fffffff},
	{0x60000000, 0x7e7fffff},
	{0x86b40000, 0x879bffff},
	{0x87fe0000, 0x87ffffff},
	{0xff000000, 0xffffffff},
	{0x100000000, 0x4767fffff},
};
*/

static long num_of_sent[NUM_OF_MEM_AREA];
static long num_of_send[NUM_OF_MEM_AREA];
static long num_of_send_point[NUM_OF_MEM_AREA];
static int mem_area_num;

static spinlock_t postcopy_lock;
#ifdef CONFIG_BITARRAY_LOCK
static spinlock_t bitarray_lock;
#endif
static bool no_request;
static bool is_transmitting;
static bool seq_thread_is_transmitting;
static bool request_recv_disrupted;
static bool fail_to_send_seq;
static bool fail_to_send_req;
static bool fail_to_send_req_dis;
//static u64 disrupted_addr;

struct echo_state
{
	u8_t state;
	u8_t retries;
	struct tcp_pcb *pcb;
	struct pbuf *p;
};

u64 t3, t4;
u64 st1, st2;
u64 seq_t1, seq_t2;
u64 t_mig[2];

u64 temp_t[30];
//u64 t_count;

u64 temp_count[20];

enum margin_type {
	MARGIN00,
	MARGIN01,
	MARGIN02,
	MARGIN03,
	MARGIN04,
	MARGIN05,
	MARGIN06SP,
	MARGIN_BUFF,
	NO_MARGIN,
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
	u64 send_size;
	u64 send_num;
	enum margin_type m_type;
	u64 bitarray_num;
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
static void
seq_transmit_thread (void *arg);

inline void get_time_wrap(u64 *time)
{
#ifdef CONFIG_POSTCOPY_MEASURE
	*time = get_cpu_time();
#endif
}

// check whether gphys is migrated or not
unsigned int get_bitarray(u64 gphys)
{
	u64 pos = gphys >> SHIFT;
	unsigned int ret = 0;
	//printf("pos: %d\n", pos);
#ifdef CONFIG_MIG_DBG
	DBGPC ("(%s) pos: %ld\n", __func__, pos);
	DBGPC ("(%s) gphys: %lx\n", __func__, gphys);
	DBGPC ("(%s) shift: %ld\n", __func__, (gphys >> MIG_UNIT) & 0x3f);
	DBGPC ("(%s) bitarray[%ld]: %lx\n", __func__, pos, bitarray[pos]);
#endif
	ret = (bitarray[pos] >> ((gphys >> MIG_UNIT) & 0x3f)) & 1;
	//return (bitarray[pos] >> ((gphys >> MIG_UNIT) & 0x3f)) & 1;
	return ret;
}

void set_bitarray(u64 gphys)
{
	unsigned int pos = gphys >> SHIFT;
	bitarray[pos] |= (1LL << ((gphys >> MIG_UNIT) & 0x3f));		// LL means 64-bit constant
#ifdef CONFIG_MIG_DBG
	DBGPC ("(%s) pos: %ld\n", __func__, pos);
	DBGPC ("(%s) gphys: %lx\n", __func__, gphys);
	DBGPC ("(%s) 1LL << ((gphys >> MIG_UNIT) & 0x3f): %lx\n", __func__, 1LL << ((gphys >> MIG_UNIT) & 0x3f));
	DBGPC ("(%s) bitarray[%ld]: %lx\n", __func__, pos, bitarray[pos]);
#endif
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

	err = tcp_write (pcb, &msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		printd ("(%s) tcp_write failed. error_code:%d\n", __func__,
				err);
		return -1;
	}
	return err;
}

static int
tcp_send_seq_msg (struct tcp_pcb *pcb)
{
	err_t err;

	err = tcp_write (pcb, &seq_msg, SIZE_OF_SENDBUF, TCP_WRITE_FLAG_MORE | TCP_WRITE_FLAG_COPY);
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

		//time_measure = true;
		get_acpi_time (&mig_start_t);
		new_start_t = mig_start_t;
	}
	return 0;
}

static err_t
request_send (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	//get_acpi_time(&st2);
	st2 = get_cpu_time();
	//printf("request send ack time: %d\n", st2 - st1);
	return 0;
}

static err_t
postcopy_requested_sent (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	//spinlock_unlock (&postcopy_lock);
	return 0;
}

/*
 * postcopy_request function sends requested gphy's data to echo-server(migrated machine)
 */
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
#ifdef CONFIG_EXT_INT_DBG
		/*if (migration_done_msg_sent) {
			printf ("%s\n", __func__);
		}*/
		if (migration_done_msg_sent) {
			printf ("%s still called\n", __func__);
		}
#endif
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
			//printf ("(%s) batch is larger than batch_size, batch: %d, requested_addr: %lx\n", __func__, batch, requested_addr);
			batch = 0;
			tcp_sent (tpcb, postcopy_requested_sent);

			bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_REQ_MEM);	// 1MB memory is sent
			ret_tcp_send = tcp_send_msg(tpcb);
			tcp_output(tpcb);
			/* if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			} */

#ifdef CONFIG_MIG_DBG
			DBGPC ("(%s) call set_bitarray\n", __func__);
#endif
#ifdef CONFIG_BITARRAY_LOCK
			spinlock_lock(&bitarray_lock);
			set_bitarray(requested_addr);
			spinlock_unlock(&bitarray_lock);
#else
			set_bitarray(requested_addr);
#endif
			no_request = true;
			//spinlock_unlock (&postcopy_lock);
			//requested = false;
			return ERR_OK;	// batch end
		} else {
#ifdef CONFIG_SENDBUF_DBG
			tcp_sent(tpcb, postcopy_requested);
#else
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, postcopy_requested);
			} else {
				tcp_sent(tpcb, NULL);
			}
#endif
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
#ifdef CONFIG_RESEND_IN_THREAD
	//if (batch == 0)
	fail_to_send_req = true;
#endif
}

static err_t
postcopy_requested_disrupted (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset;
	int ret_tcp_send = -1;

	//get_acpi_time(&st2);
	//printf("request send ack time: %d\n", st2 - st1);
	// send 1024 packets
	while (tcp_sendbuf_available(tpcb)) {
#ifdef CONFIG_EXT_INT_DBG
		if (migration_done_msg_sent) {
			printf ("%s is still called\n", __func__);
		}
#endif
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
			//printf ("(%s) batch is larger than batch_size, batch: %d, requested_addr: %lx\n", __func__, batch, requested_addr);
			batch = 0;
			tcp_sent (tpcb, postcopy_requested_sent);

			bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_REQ_MEM);	// 1MB memory is sent
			ret_tcp_send = tcp_send_msg(tpcb);
			tcp_output(tpcb);
			/* if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			} */

#ifdef CONFIG_MIG_DBG
			DBGPC ("(%s) call set_bitarray\n", __func__);
#endif
#ifdef CONFIG_BITARRAY_LOCK
			spinlock_lock(&bitarray_lock);
			set_bitarray(requested_addr);
			spinlock_unlock(&bitarray_lock);
#else
			set_bitarray(requested_addr);
#endif
			no_request = true;
			request_recv_disrupted = false;
			//DBGPC ("(%s) addr %lx\n", __func__, requested_addr);
			//spinlock_unlock (&postcopy_lock);
			//requested = false;
			return ERR_OK;	// batch end
		} else {
#ifdef CONFIG_SENDBUF_DBG
			tcp_sent(tpcb, postcopy_requested_disrupted);
#else
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, postcopy_requested_disrupted);
			} else {
				tcp_sent(tpcb, NULL);
			}
#endif
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
#ifdef CONFIG_RESEND_IN_THREAD
	//if (batch == 0) 
	fail_to_send_req_dis = true;
#endif
}


static void set_multiple_bitarray (u64 start_addr, u64 end_addr)
{
	u64 size_area = (end_addr - start_addr + 1) >> MIG_UNIT;
	for (int i = 0; i < size_area; i++)
	{
#ifdef CONFIG_MIG_DBG
		DBGPC ("(%s) call set_bitarray\n", __func__);
#endif
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_lock(&bitarray_lock);
		set_bitarray((start_addr & 0xfffffffffffe0000) + i * ARRAY_MULTIPLIER);
		spinlock_unlock(&bitarray_lock);
#else
		set_bitarray((start_addr & 0xfffffffffffe0000) + i * ARRAY_MULTIPLIER);
#endif
	}
}

static err_t
margins_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	//spinlock_unlock (&postcopy_lock);
	// this is called multiple time
	return 0;
}

static err_t
test_send_sub_do_nothing (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	temp_t[1] = get_cpu_time();
	//printf ("elapsed time: %d us\n", temp_t[1] - temp_t[0]);
	return 0;
}

static err_t
test_send_sub (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset = 0;
	int ret_tcp_send = -1;
	char *data;

	while (tcp_sendbuf_available(tpcb)) {
#ifdef CONFIG_EXT_INT_DBG
		if (migration_done_msg_sent) {
			printf ("%s is still called\n", __func__);
		}
#endif
		offset = batch * SIZE_SENDBUF_DATA;
		data = msg.meta.data_mem.addr_to_migrate + offset;
		//printf ("(%s) offset: %d, data: %lx\n", __func__, offset, data);
		memcpy (msg.data, data, SIZE_SENDBUF_DATA);
		msg.meta.data_mem.batch_num = batch;
		batch++;

		if (batch >= batch_size_param) {
			//printf("(%s), batch in batch >= BATCH_SIZE: %d\n", __func__, batch);
			tcp_sent (tpcb, test_send_sub_do_nothing);
			batch = 0;
			
			bf_msg_set_proto_id(&msg, PROTO_ID_TEST_SEND_END);
			ret_tcp_send = tcp_send_msg(tpcb);
			tcp_output(tpcb);

			return ERR_OK;
		} else {
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
			//if (tcp_sndbuf(tpcb) < SIZE_OF_SENDBUF + 512) {	// TODO
				// SIZE_OF_SENDBUF + 24 doesn't send all 128KB
				// SIZE_OF_SENDBUF + 56 doesn't send all 128KB (maybe last packet don't go)
				// SIZE_OF_SENDBUF + 128, 512 doesn't send all 128KB
				temp_count[0]++;
				tcp_sent(tpcb, test_send_sub);
			} else {
				//printf ("tcp_sent is set to NULL\n");
				temp_count[1]++;
				tcp_sent(tpcb, NULL);
			}
			ret_tcp_send = tcp_send_msg(tpcb);
			//printf("(%s), batch: %d\n", __func__, batch);
		}
	}
}

// This implementation stop the system
static err_t
test_send_sub2 (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned int batch = 0;
	static unsigned long offset = 0;
	int ret_tcp_send = -1;
	char *data;

	while (1) {
		if (tcp_sendbuf_available(tpcb)) {
			offset = batch * SIZE_SENDBUF_DATA;
			data = msg.meta.data_mem.addr_to_migrate + offset;
			memcpy (msg.data, data, SIZE_SENDBUF_DATA);
			msg.meta.data_mem.batch_num = batch;
			batch++;
			printf ("batch: %d\n", batch);

			if (batch >= batch_size_param) {
				temp_count[0]++;
				tcp_sent (tpcb, test_send_sub_do_nothing);
				batch = 0;
				bf_msg_set_proto_id(&msg, PROTO_ID_TEST_SEND_END);
				ret_tcp_send = tcp_send_msg(tpcb);
				tcp_output(tpcb);

				return ERR_OK;
			} else {
				temp_count[1]++;
				tcp_sent (tpcb, NULL);
				ret_tcp_send = tcp_send_msg(tpcb);
			}
		} else {
			temp_count[2]++;
			tcp_output(tpcb);
		}
	}
}

err_t
test_send (ulong param)
{
	struct tcp_pcb *tpcb = echo_client_pcb;
	// send here, size = param KB
	u64 addr = alloc (param * SEND_SIZE);	// You have to free it later

	temp_t[0] = get_cpu_time();

	bf_msg_set_proto_id(&msg, PROTO_ID_TEST_SEND);
	msg.meta.data_mem.addr_to_migrate = addr;
	//msg.meta.data_mem.batch_size = BATCH_SIZE;
	msg.meta.data_mem.batch_size = param;
	batch_size_param = param;

	tcp_sent(echo_client_pcb, test_send_sub);
	test_send_sub (NULL, echo_client_pcb, 0);

	/* This implementation is stuck at batch 62 when test send 128
	tcp_sent(echo_client_pcb, NULL);
	test_send_sub2 (NULL, echo_client_pcb, 0);*/

	return ERR_OK;
}

void
print_network (ulong param)
{
	int i = 0;

	switch (param) {
		case 0:
			printf ("elapsed time: %d us\n", temp_t[1] - temp_t[0]);
			printf ("temp_count[0]: %d\n"
				"temp_count[1]: %d\n"
				"temp_count[2]: %d\n", temp_count[0], temp_count[1], temp_count[2]);
			for (i = 0; i < 20; i++) {
				temp_count[i] = 0;
			}
			break;
		case 1:
			printf ("tcp sndbuf is less than 2048: %d\n"
				"tcp sndbuf is larger than 2048: %d\n", temp_count[0], temp_count[1]);
			for (i = 0; i < 20; i++) {
				temp_count[i] = 0;
			}
			break;
		case 2:
			printf ("REQUEST_MASK: %lx\n"
				"ARRAY_MULTIPLIER: %lx\n"
				"MARGIN_OFFSET: %lx\n", REQUEST_MASK, ARRAY_MULTIPLIER, MARGIN_OFFSET);
			break;
		case 3:
			printf ("background copy time: %ldus\n", t_mig[1] - t_mig[0]);
			printf ("background copy time in s: %lds\n", (t_mig[1] - t_mig[0]) / 1000000);
			break;
		default:
			break;
	}
}

static err_t
margin_batch_end(struct tcp_pcb *tpcb)
{
	int ret_tcp_send = -1;
		
#ifdef CONFIG_MIG_DBG
	DBGPC ("(%s) call set_bitarray\n", __func__);
#endif
#ifdef CONFIG_BITARRAY_LOCK
	spinlock_lock(&bitarray_lock);
	set_bitarray (postcopy_state.start_addr);
	spinlock_unlock(&bitarray_lock);
#else
	set_bitarray (postcopy_state.start_addr);
#endif
	tcp_sent (tpcb, send_margins);
	ret_tcp_send = tcp_send_seq_msg (tpcb);
	tcp_output(tpcb);

	if (ret_tcp_send != ERR_OK) {
		printd("(%s) tcp_send_buf failed. error code:%d\n", 
				__func__, ret_tcp_send);
		return -1;
	}
	return ERR_OK;
}

static err_t
migration_done_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	migration_done_msg_sent = true;
}

static err_t
send_migration_done (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	int ret_tcp_send = -1;

	if (tcp_sendbuf_available(tpcb)) {
		bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_MIG_DONE);
		//tcp_sent (tpcb, NULL);
		tcp_sent (tpcb, migration_done_sent);
		ret_tcp_send = tcp_send_seq_msg (tpcb);
		tcp_output(tpcb);

		if (ret_tcp_send != ERR_OK) {
			printd("(%s) tcp_send_buf failed. error code:%d\n", 
					__func__, ret_tcp_send);
		} else {
			//migration_done_msg_sent = true;
		}
		return ERR_OK;
	} else {
		printf ("(%s) Not enough sendbuf\n", __func__);
	}
}

static err_t
send_margin_less_than_1kb (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned long offset;
	int ret_tcp_send = -1;

	if (tcp_sendbuf_available(tpcb)) {
		offset = (unsigned long)(postcopy_state.start_addr - usable_mem_list[postcopy_state.mem_area].start_addr) >> 3;
		memcpy (seq_msg.data, usable_vmem_list[postcopy_state.mem_area] + offset, 762);
		seq_msg.meta.data_mem.batch_num = 0;
		postcopy_state.m_type = MARGIN_BUFF;
		tcp_sent (tpcb, send_margins);
		ret_tcp_send = tcp_send_seq_msg (tpcb);
		tcp_output(tpcb);
		// change proto id here? FIXME yeah, you have to do this.
		bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_DATA_MEM);

		if (ret_tcp_send != ERR_OK) {
			printd("(%s) tcp_send_buf failed. error code:%d\n", 
					__func__, ret_tcp_send);
		}
		return ERR_OK;
	} else {
		printf ("(%s) Not enough sendbuf\n", __func__);
	}
}

static err_t
send_margin (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	static unsigned long offset;
	static unsigned int batch = 0;
	int ret_tcp_send = -1;
	//DBGPC ("(%s) batch: %d, before while loop\n", __func__, batch);
	
	while (tcp_sendbuf_available(tpcb)) {
#ifdef CONFIG_EXT_INT_DBG
		if (migration_done_msg_sent) {
			printf ("%s is still called\n", __func__);
		}
#endif
		offset = (unsigned long)(postcopy_state.start_addr - usable_mem_list[postcopy_state.mem_area].start_addr + postcopy_state.send_num * SIZE_SENDBUF_DATA) >> 3;
		memcpy (seq_msg.data, usable_vmem_list[postcopy_state.mem_area] + offset, SIZE_SENDBUF_DATA);
		seq_msg.meta.data_mem.batch_num = batch;
		postcopy_state.send_num++;
		batch++;
		//DBGPC ("(%s) batch: %d\n", __func__, batch);

		if (postcopy_state.send_num >= postcopy_state.send_size) {
			switch (postcopy_state.m_type) {
				case MARGIN00:
					batch = 0;
					postcopy_state.m_type = MARGIN01;	// next margin
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN01:
					batch = 0;
					postcopy_state.m_type = MARGIN02;	// next margin
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN02:
					batch = 0;
					postcopy_state.m_type = MARGIN03;
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN03:
					batch = 0;
					postcopy_state.m_type = MARGIN04;
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN04:
					batch = 0;
					postcopy_state.m_type = MARGIN05;
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN05:
					batch = 0;
					postcopy_state.m_type = MARGIN06SP;
					ret_tcp_send = margin_batch_end(tpcb);
					if (ret_tcp_send != ERR_OK)
						return -1;
					return ERR_OK;
					break;
				case MARGIN_BUFF:
					batch = 0;
					postcopy_state.m_type = NO_MARGIN;
					DBGPC ("(%s) case of MARGIN_BUFF\n", __func__);
#ifdef CONFIG_BITARRAY_LOCK
					spinlock_lock(&bitarray_lock);
					set_bitarray (postcopy_state.start_addr);
					spinlock_unlock(&bitarray_lock);
#else
					set_bitarray (postcopy_state.start_addr);
#endif
					tcp_sent (tpcb, margins_sent);	// different ack func
					bf_msg_set_proto_id (&seq_msg, PROTO_ID_META_BUFF_MEM);
					ret_tcp_send = tcp_send_seq_msg (tpcb);
					tcp_output(tpcb);
					if (ret_tcp_send != ERR_OK) {
						printd("(%s) tcp_send_buf failed. error code:%d\n", 
								__func__, ret_tcp_send);
						return -1;
					}

//#ifdef BACKGROUND_COPY
					DBGPC ("(%s) thread_new called\n", __func__);
					time_measure = false;
					get_time_wrap(&t_mig[0]);
					thread_new (seq_transmit_thread, NULL, VMM_STACKSIZE);
//#endif

					return ERR_OK;	// batch end

					break;
				case NO_MARGIN:
					/*
					if (postcopy_state.send_num > postcopy_state.send_size) {
						// exception processing
						// Don't init batch = 0
						
					}*/
					batch = 0;
					tcp_sent (tpcb, margins_sent);
					if(requested_addr == postcopy_state.start_addr) {
						printf ("requested_addr is the same with seq addr\n");
						bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_DATA_REQ_MEM);
					}
					/*DBGPC("(%s), addr_to_migrate: %lx\n", __func__, 
							seq_msg.meta.data_mem.addr_to_migrate);*/
					ret_tcp_send = tcp_send_seq_msg (tpcb);
					tcp_output(tpcb);
					if (ret_tcp_send != ERR_OK) {
						printd("(%s) tcp_send_buf failed. error code:%d\n", 
								__func__, ret_tcp_send);
						return -1;
					}
					is_transmitting = false;
					seq_thread_is_transmitting = false;
					return ERR_OK;	// batch end

					// set_bitarray in postcopy_seq_task()
					break;
				default:
					printf ("no default happen\n");
					break;
			}

			/*ret_tcp_send = tcp_send_seq_msg (tpcb);
			tcp_output(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_buf failed. error code:%d\n", __func__, ret_tcp_send);
				return -1;
			}

			return ERR_OK;	// batch end*/
		} else {
#ifdef CONFIG_SENDBUF_DBG
			tcp_sent(tpcb, send_margin);
#else
			if (tcp_sndbuf(tpcb) < 2*SIZE_OF_SENDBUF) {
				tcp_sent(tpcb, send_margin);
			} else {
				tcp_sent(tpcb, NULL);
			}
#endif
			printd ("(%s) send batch\n", __func__);
			ret_tcp_send = tcp_send_seq_msg(tpcb);
			if (ret_tcp_send != ERR_OK) {
				printd("(%s) tcp_send_msg failed. error code:%d\n", 
						__func__, ret_tcp_send);
				return -1;
			}
		}

		/*if (batch == 1024) {	// 1024 for 1MB
			batch = 0;
			seq_msg.meta.data_mem.addr_to_migrate += 0x100000;
		}*/
	}
#ifdef CONFIG_RESEND_IN_THREAD
	// If the code arrives here, fail_to_send_seq
	//if (batch == 0)
	fail_to_send_seq = true;
#endif
}

void set_send_margin (u64 start_addr, u64 end_addr)
{
	int i = 0;
	postcopy_state.start_addr = start_addr;
	postcopy_state.end_addr = end_addr;
	postcopy_state.send_size = (end_addr - start_addr + 1)/SEND_SIZE;
	if (postcopy_state.send_size == 0)
		postcopy_state.send_size = 1;

	for(i = 0; i < NUM_OF_MEM_AREA; i++) {
		if(start_addr >= usable_mem_list[i].start_addr && end_addr <= usable_mem_list[i].end_addr) {
			postcopy_state.mem_area = i;
			break;
		}
	}

	postcopy_state.send_num = 0;
	postcopy_state.bitarray_num = (end_addr - start_addr) >> MIG_UNIT;	// don't care

	//bf_msg_set_proto_id (&msg, PROTO_ID_META_DATA_MEM);
	seq_msg.meta.data_mem.addr_to_migrate = start_addr;
	/*DBGPC("(%s), addr_to_migrate: %lx\n", __func__, 
			seq_msg.meta.data_mem.addr_to_migrate);*/

	seq_msg.meta.data_mem.batch_size = postcopy_state.send_size;
	/*DBGPC("(%s), batch_size: %d\n", __func__, 
			seq_msg.meta.data_mem.batch_size);*/
}

/* send_margins() selects memory area which is not fit for batch size (currently 32KB)
 * Our migration scheme sends 
 */
static err_t
send_margins (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	static bool margin00 = false;
	static bool margin01 = false;
	static bool margin02 = false;
	static bool margin03 = false;
	static bool margin04 = false;
	static bool margin05 = false;
	static bool margin06sp = false;
	static bool margin_buff = false;

	switch (postcopy_state.m_type) {
		case MARGIN00:
			if (margin00) {
				return;
			} else {
				DBGPC ("(%s) MARGIN00 send\n", __func__);
				set_send_margin (margin_list[0].start_addr, margin_list[0].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin00 = true;
			}
			break;
		case MARGIN01:
			if (margin01) {
				return;
			} else {
				DBGPC ("(%s) MARGIN01 send\n", __func__);
				set_send_margin (margin_list[1].start_addr, margin_list[1].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin01 = true;
			}
			break;
		case MARGIN02:
			if (margin02) {
				return;
			} else {
				DBGPC ("(%s) MARGIN02 send\n", __func__);
				set_send_margin (margin_list[2].start_addr, margin_list[2].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin02 = true;
			}
			break;
		case MARGIN03:
			if (margin03) {
				return;
			} else {
				DBGPC ("(%s) MARGIN03 send\n", __func__);
				set_send_margin (margin_list[3].start_addr, margin_list[3].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin03 = true;
			}
			break;
		case MARGIN04:
			if (margin04) {
				return;
			} else {
				DBGPC ("(%s) MARGIN04 send\n", __func__);
				set_send_margin (margin_list[4].start_addr, margin_list[4].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin04 = true;
			}
			break;
		case MARGIN05:
			if (margin05) {
				return;
			} else {
				DBGPC ("(%s) MARGIN05 send\n", __func__);
				set_send_margin (margin_list[5].start_addr, margin_list[5].end_addr);
				send_margin (NULL, echo_client_pcb, 0);
				margin05 = true;
			}
			break;
		case MARGIN06SP:
			if (margin06sp) {
				return;
			} else {
				margin06sp = true;
				DBGPC ("(%s) MARGIN06 send\n", __func__);
				set_send_margin (margin_list[6].start_addr, margin_list[6].end_addr);
				bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_DATA_MEM_SP);
				postcopy_state.send_size = 1;
				seq_msg.meta.data_mem.batch_size = 1;
				send_margin_less_than_1kb (NULL, echo_client_pcb, 0);
			}
			break;
		case MARGIN_BUFF:
			if (margin_buff) {
				return;
			} else {
				DBGPC ("(%s) MARGIN_BUFF send\n", __func__);
				/*if ((((u64)virtio_recv_queue << 12) & 0xfffffffffffe0000) 
						!= (((u64)virtio_send_queue << 12) & 0xfffffffffffe0000))
					printf ("Error: virtio_recv_queue and send_queue " 
							"masking results are not equal!!!\n");
				else {*/
					//virtio_queue64 = ((u64)virtio_send_queue << 12) & 0xfffe0000;
					virtio_queue64 = ((u64)virtio_recv_queue << 12) & 0xfffe0000;
					//set_send_margin (virtio_queue64, virtio_queue64 + 0x1ffff);
					set_send_margin (virtio_queue64, virtio_queue64 + 0x5ffff);
					DBGPC ("(%s) call send_margin in case of MARGIN_BUFF\n", __func__);
					send_margin (NULL, echo_client_pcb, 0);
				//}
				margin_buff = true;
			}
			break;
		//case NO_MARGIN:
			//break;
		default:
			break;
	}

	/*
	if (postcopy_state.m_type == MARGIN02) {
		if (margin02 == true)
			return;
		else {
			printf ("MARGIN02 send\n");
			t_margin[2][0] = get_cpu_time();
			set_send_margin (0x86b35000, 0x86b3ffff);
			margin02 = true;
		}
		//set_multiple_bitarray (0x86b35000, 0x86bfffff);	// 0x86b35000, 0x86b3ffff
		//set_bitarray here
		//get_acpi_time (&t_margin[2][1]);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN04) {
		if (margin04 == true)
			return;
		else {
			printf ("MARGIN04 send\n");
			t_margin[4][0] = get_cpu_time();
			set_send_margin (0x87fcd000, 0x87fdffff);
			margin04 = true;
		}
		//set_multiple_bitarray (0x87fcd000, 0x87ffffff);	// 0x87fcd000, 0x87fdffff
		//set_bitarray here
		//get_acpi_time (&t_margin[4][1]);
		send_margin (NULL, echo_client_pcb, 0);
	} else if (postcopy_state.m_type == MARGIN_BUFF) {	// virtio buffers
		if (margin_buff == true)
			return;
		else {
			printf ("BUFF_MEM send\n");
			//get_acpi_time (&t_margin[6][0]);
			if ((((u64)virtio_recv_queue << 12) & 0xfffffffffffe0000) 
					!= (((u64)virtio_send_queue << 12) & 0xfffffffffffe0000))
				printf ("Error: virtio_recv_queue and send_queue " 
						"masking results are not equal!!!\n");
			else {
				virtio_queue64 = ((u64)virtio_send_queue << 12) & 0xfffe0000;
				set_send_margin (virtio_queue64, virtio_queue64 + 0x1ffff);
				//set_multiple_bitarray (virtio_queue64, virtio_queue64 + 0x1ffff);
				//get_acpi_time (&t_margin[6][1]);
				send_margin (NULL, echo_client_pcb, 0);
			}
			margin_buff = true;
		}
	}
	*/
}

static u64 get_next_addr(u64 start_addr)
{
	start_addr += ADDR_OFFSET;		// 128KB
#ifdef CONFIG_DEBUG_GET_NEXT_ADDR
	if (start_addr == usable_mem_list_gran[0].end_addr + 1) {
		start_addr = usable_mem_list_gran[1].start_addr;
	} else if (start_addr == usable_mem_list_gran[1].end_addr + 1) {
		start_addr = 0xffffffffffffffff;
	}
#else
	if (start_addr == usable_mem_list_gran[0].end_addr + 1) {
		start_addr = usable_mem_list_gran[1].start_addr;
	} else if (start_addr == usable_mem_list_gran[1].end_addr + 1) {
		start_addr = usable_mem_list_gran[2].start_addr;
	} else if (start_addr == usable_mem_list_gran[2].end_addr + 1) {
		start_addr = usable_mem_list_gran[3].start_addr;
	} else if (start_addr == usable_mem_list_gran[3].end_addr + 1) {
		start_addr = usable_mem_list_gran[4].start_addr;
	} else if (start_addr == usable_mem_list_gran[4].end_addr + 1) {
		start_addr = usable_mem_list_gran[5].start_addr;
	} else if (start_addr == usable_mem_list_gran[5].end_addr + 1) {
		start_addr = usable_mem_list_gran[6].start_addr;
	} else if (start_addr == usable_mem_list_gran[6].end_addr + 1) {
		start_addr = 0xffffffffffffffff;
	}
#endif
	return start_addr;
}

static u64 get_start_addr()
{
	static u64 start_addr = 0;
	u64 ret = 0;

	if(start_addr == 0xffffffffffffffff) {
		printf ("start_addr = 0xffffffffffffffff\n");
		return start_addr;	// return 0xffffffffffffffff
	}

	for(;;) {
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_lock(&bitarray_lock);
#endif
		if(get_bitarray(start_addr)) {	// already sent
#ifdef CONFIG_BITARRAY_LOCK
			spinlock_unlock(&bitarray_lock);
#endif

#ifdef BACKGROUND_COPY_DBG
			printf("get_bitarray returns 1, start_addr: %lx\n", start_addr);
#endif
			start_addr = get_next_addr(start_addr);		// + 0x20000

			if(start_addr == 0xffffffffffffffff) {
				printf ("start_addr = 0xffffffffffffffff\n");
				return start_addr;	// return 0xffffffffffffffff
			}
		} else {
#ifdef CONFIG_BITARRAY_LOCK
			spinlock_unlock(&bitarray_lock);
#endif
			break;
		}
	}

	ret = start_addr;

	start_addr = get_next_addr(start_addr);
	return ret;
}

/* select_mem_area func is used in postcopy_requested func
*/
void select_mem_area()
{
	if (requested_addr >= usable_mem_list[6].start_addr)
		req_mem_area_num = 6;
	else if (requested_addr <= usable_mem_list[0].end_addr)
		req_mem_area_num = 0;
	else if (requested_addr <= usable_mem_list[1].end_addr)
		req_mem_area_num = 1;
	else if (requested_addr <= usable_mem_list[2].end_addr)
		req_mem_area_num = 2;
	else if (requested_addr <= usable_mem_list[3].end_addr)
		req_mem_area_num = 3;
	else if (requested_addr <= usable_mem_list[4].end_addr)
		req_mem_area_num = 4;
	else
		req_mem_area_num = 5;

	requested_addr = requested_addr & REQUEST_MASK;
	requested_offset = requested_addr - usable_mem_list[req_mem_area_num].start_addr;
}

static void send_disrupted_addr(void)
{
#ifdef CONFIG_BITARRAY_LOCK
	spinlock_lock(&bitarray_lock);
#endif
	if(get_bitarray(requested_addr)) {
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_unlock(&bitarray_lock);
#endif
		request_recv_disrupted = false;
		/*printf("(%s) already sent by seq thread: %lx\n", 
				__func__, requested_addr);*/
		postcopy_stat[2]++;
	} else {
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_unlock(&bitarray_lock);
#endif
		//requested_addr = disrupted_addr;
		select_mem_area();
		bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_MEM);
		msg.meta.data_mem.addr_to_migrate = requested_addr;
		msg.meta.data_mem.batch_size = BATCH_SIZE;
		no_request = false;
		DBGPC ("(%s) call postcopy_requested_disrupted\n", __func__);

		tcp_sent(echo_client_pcb, postcopy_requested_disrupted);
		postcopy_requested_disrupted (NULL, echo_client_pcb, 0);
		postcopy_stat[1]++;
	}
}

static bool postcopy_seq_task ()
{
	u64 start_addr = 0;
#ifdef CONFIG_DBG_COUNTING
	static u64 counting = 0;
#endif
	is_transmitting = true;
	seq_thread_is_transmitting = true;
	//spinlock_lock (&postcopy_lock);
	start_addr = get_start_addr();

#ifdef CONFIG_DBG_COUNTING
	counting++;
	if (counting % 20000 == 0)
		printf ("(%s) addr: %lx\n", __func__, start_addr);
#endif
	if(start_addr == 0xffffffffffffffff) {
		is_transmitting = false;
		seq_thread_is_transmitting = false;

		get_time_wrap(&t_mig[1]);
		send_migration_done(NULL, echo_client_pcb, 0);
		printf("(%s) migration done\n", __func__);
		// exit background migration thread
		migration_done = true;
		return true;
	}
	set_send_margin(start_addr, start_addr + MARGIN_OFFSET);
	bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_DATA_MEM);

#ifdef CONFIG_BITARRAY_LOCK
	spinlock_lock(&bitarray_lock);
	set_bitarray(start_addr);
	spinlock_unlock(&bitarray_lock);
#else
	set_bitarray(start_addr);
#endif
	DBGPC("(%s) send_margin %lx\n", __func__, start_addr);
	send_margin(NULL, echo_client_pcb, 0);
	return false;
}

static void
print_migration_stat (void)
{
	printf ("---Migration is done and msg is sent to migrated machine----\n");
	printf ("-----Let's me show the statistics of postcopy migration-----\n");
	printf ("background copy time: %ldus\n", t_mig[1] - t_mig[0]);
	printf ("background copy time in s: %lds\n", (t_mig[1] - t_mig[0]) / 1000000);
	printf ("background copy time in minute: %ld minutes\n", (t_mig[1] - t_mig[0]) / 1000000 / 60);
	printf ("\n");
	printf ("The number of request recved: %lld\n", postcopy_stat[0]);
	printf ("The number of reply sent: %lld\n", postcopy_stat[1]);
	printf ("Already sent by background thread: %lld\n", postcopy_stat[2]);
	//printf ("\n", postcopy_stat[3]);
	printf ("-----------------------Statistics END-----------------------\n");
	printf ("\n");
}

static void
seq_transmit_thread (void *arg)
{
	bool thread_exit_bool;
	static unsigned int sched_var = 0;
	printf ("Background copy thread start\n");
	for (;;) {
		if (migration_done) {
			if (migration_done_msg_sent == false) {
				send_migration_done(NULL, echo_client_pcb, 0);
			} else {
				// migraiton is done and msg also sent to echo-server (migrated machine)
				print_migration_stat();
				break;
			}
		} else {
			if (no_request == true && is_transmitting == false) {
				//seq_t1 = get_cpu_time();
				//printf("seq_task period: %d\n", seq_t1 - seq_t2);
				//seq_t2 = seq_t1;
				// Do not measure time here.
				if (request_recv_disrupted && !seq_thread_is_transmitting) {
					//DBGPC ("(%s) let's call send_disrupted_addr \n", __func__);
					send_disrupted_addr();
				} else {
#ifdef BACKGROUND_COPY
#ifdef CONFIG_BACKGROUND_COPY_LIMIT
					sched_var++;
					if (sched_var % 2 == 0) {
						thread_exit_bool = postcopy_seq_task();
					}
#else
					thread_exit_bool = postcopy_seq_task();
#endif
#endif
				}
			} 
#ifdef CONFIG_RESEND_IN_THREAD
			else if (no_request == true && is_transmitting) {
				if (fail_to_send_seq) {
					//PRINT_DBG ("(%s) fail_to_send_seq is true\n", __func__);
					fail_to_send_seq = false;
					send_margin(NULL, echo_client_pcb, 0);
				}
			} else if (no_request == false) {
				if (request_recv_disrupted) {
					if (fail_to_send_req_dis) {
						//PRINT_DBG ("(%s) fail_to_send_req_dis is true\n", __func__);
						fail_to_send_req_dis = false;
						postcopy_requested_disrupted (NULL, echo_client_pcb, 0);
					}
				} else {
					if (fail_to_send_req) {
						//PRINT_DBG ("(%s) fail_to_send_req is true\n", __func__);
						fail_to_send_req = false;
						postcopy_requested (NULL, echo_client_pcb, 0);
					}
				}
			}
#endif
		}
		schedule ();
	}
	printf ("(%s) exit\n", __func__);
	thread_exit();
}

static err_t
cpu_recv_callback (void * arg, struct tcp_pcb *tpcb, u16_t len)
{
	t_margin[0][0] = get_cpu_time();
	//spinlock_unlock (&postcopy_lock);
	
	bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_MEM);
	bf_msg_set_proto_id(&seq_msg, PROTO_ID_META_DATA_MEM);
	//postcopy_state.m_type = MARGIN02;
	postcopy_state.m_type = MARGIN00;
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

	//spinlock_lock (&postcopy_lock);
	bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_CPU);

	printd ("(%s) set_send_msg_for_cpu\n", __func__);
	set_send_msg_for_cpu (mem);

	printd ("(%s) tcp_send_msg\n", __func__);
	//printf ("wait here\n");

	//get_acpi_time (&t_start[1]);
	t_start[1] = get_cpu_time();
	printd_t ("(%s) send CPU state\n", __func__);
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

/*
static void print_client_root_cause_overhead (void)
{
	u64 margins_sum = 0;
	int i = 0;
	printf ("client root cause of overhead start\n");
	printf ("cpu send %ld\n", t_start[1] - t_start[0]);
	for (i = 0; i < 7; i++) {
		margins_sum += (t_margin[i][1] - t_margin[i][0]);
	}
	printf ("margin sum %ld\n", margins_sum);
	for (i = 0; i < EXPR_NUM; i++) {
		printf ("%d send time %ld\n", i, t_creq[i][1] - t_creq[i][0]);
	}
}
*/

static err_t
request_recv (void)
{
#ifdef CONFIG_BITARRAY_LOCK
	spinlock_lock(&bitarray_lock);
#endif
	if(get_bitarray(requested_addr)) {
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_unlock(&bitarray_lock);
#endif
		//printf("already sent by seq thread: %lx\n", requested_addr);
		postcopy_stat[2]++;
		return ERR_OK;	// already sent by sequential thread
	} else {	// should send
		static int i = 0;
#ifdef CONFIG_BITARRAY_LOCK
		spinlock_unlock(&bitarray_lock);
#endif
		if (seq_thread_is_transmitting) {
			DBGPC ("(%s) seq_thread_is_transmitting\n", __func__);
			request_recv_disrupted = true;
			//disrupted_addr = requested_addr;
			return ERR_OK;
		} else {
			select_mem_area();
			bf_msg_set_proto_id(&msg, PROTO_ID_META_DATA_MEM);
			DBGPC ("(%s) requested_addr: %lx\n", __func__, requested_addr);
			msg.meta.data_mem.addr_to_migrate = requested_addr;
			msg.meta.data_mem.batch_size = BATCH_SIZE;	// 1024
			//spinlock_lock (&postcopy_lock);
			//t4 = get_cpu_time();
			no_request = false;
			//printf("recv ~ request_recv func: %d\n", t4-t3);
			// get_acpi_time here FIXME 2018-11-09
			//get_acpi_time (&t_creq[i++][1]);
			/*
			   if (i > EXPR_NUM - 1) {
			   print_client_root_cause_overhead ();
			   }*/	// FIXME
			tcp_sent(echo_client_pcb, postcopy_requested);
			postcopy_requested (NULL, echo_client_pcb, 0);
			postcopy_stat[1]++;

			return ERR_OK;
		}
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
		// get_acpi_time here FIXME 2018-11-09 request recved
		recv_msg = p->payload;
		if (bf_msg_get_proto_id(recv_msg) == PROTO_ID_META_DATA_REQ_MEM) {
			static int i = 0;
			//get_acpi_time(&t_creq[i++][0]);
			//printf("requested true\n");
			//requested = true;
			requested_addr = (u64)((*recv_msg).meta.data_mem.addr_to_migrate);
			DBGPC("receive, requested_addr: %lx\n", requested_addr);
			postcopy_stat[0]++;
			request_recv ();
		} else if (bf_msg_get_proto_id(recv_msg) == PROTO_ID_META_START_POSTCOPY) {
			//get_acpi_time (&t_start[0]);
			t_start[0] = get_cpu_time();
			dont_start_postcopy = false;
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
	requested_addr = 0xffffffffffffffff;
	no_request = false;
	fail_to_send_seq = false;
	fail_to_send_req = false;
	fail_to_send_req_dis = false;
	migration_done = false;
	migration_done_msg_sent = false;
	bf_msg_init_msg(&msg);
	bf_msg_init_msg(&seq_msg);
	init_message = 0;
	dont_start_postcopy = true;
	memset(bitarray, 0, BITARRAY*8);	// 8-Byte * 4096
#ifdef CONFIG_BITARRAY_LOCK
	spinlock_init(&bitarray_lock);
#endif

	printf("mig_done value at echo_client_init: %d\n", mig_done);
}
