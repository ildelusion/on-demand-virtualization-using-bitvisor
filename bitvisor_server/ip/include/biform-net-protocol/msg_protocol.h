#ifndef __BIFORM_MSG_PROTOCOL_H
#define __BIFORM_MSG_PROTOCOL_H

//#include <string.h>	// to memcpy

/* Protocol Id */
#define PROTO_ID_META_NODE	1
#define PROTO_ID_META_UMEM	2
#define PROTO_ID_META_DATA_CPU	3
#define PROTO_ID_META_DATA_MEM	4
#define PROTO_ID_META_DATA_REQ_MEM 	5	// if proto id 5 received, it is requested addr
#define PROTO_ID_META_DATA_KERN_MEM	6
#define PROTO_ID_META_DATA_IDT_MEM	7
#define PROTO_ID_META_DATA_2_MEM	8
#define PROTO_ID_META_DATA_0_MEM	9
#define PROTO_ID_META_DATA_3_MEM	10
#define PROTO_ID_META_DATA_4_MEM	11
#define PROTO_ID_META_TEST		12
#define PROTO_ID_META_MIG_DONE		13
#define PROTO_ID_META_BUFF_MEM		14
#define PROTO_ID_META_START_POSTCOPY	15
#define PROTO_ID_META_DATA_ZERO		16
//#define PROTO_ID_META_DATA_SEQ_MEM      17
//#define PROTO_ID_META_DATA_SEQ_REQ_MEM  18
#define PROTO_ID_TEST_SEND		19
#define PROTO_ID_TEST_SEND_END		20
#define PROTO_ID_META_DATA_MEM_SP	21

#define SIZE_SENDBUF_DATA 1024	// size of data part in send buf.
//#define SIZE_SENDBUF_DATA 1536	// size of data part in send buf.

/**
 * meta of node (machine).
 */
typedef struct meta_node 
{
    unsigned int proto_id;	 		// protocol id.
    unsigned long long int total_mem_size;	/* total mem size in the node (e.g. 4GB).
						 * server reserves this amount backing store space.
						 */
    unsigned int umem_count;		 // usable mem area count.
} __attribute__ ((packed)) meta_node_t;

static void
init_meta_node(meta_node_t* mn)
{
    mn->proto_id = 0;
    mn->total_mem_size = 0;
    mn->umem_count = 0;
}

/**
 * meta of usable memory.
 */
typedef struct meta_umem
{
    unsigned int proto_id;	 				// protocol id.
    unsigned int umem_id;					// usable mem id.
    unsigned long long int umem_size;		// this usable mem size.
    unsigned long long int start_addr;		// start address of this umem.
} __attribute__ ((packed)) meta_umem_t;

static void
init_meta_umem(meta_umem_t* mu)
{
    mu->proto_id = 0;
    mu->umem_id = 0;
    mu->umem_size = 0;
    mu->start_addr = 0;
}

/* meta basic structure.
 * It has only protocol id.
 * It is used for data_cpu
 */
typedef struct meta_basic
{
    unsigned int proto_id;	 				// protocol id.
} __attribute__ ((packed)) meta_basic_t;

static void
init_meta_basic(meta_basic_t* mb)
{
    mb->proto_id = 0;
}

/**
 * meta_data_cpu
 */
typedef struct meta_basic meta_data_cpu_t;	// It is same as meta_basic currently.

static void
init_meta_data_cpu(meta_data_cpu_t* mc)
{
    mc->proto_id = 0;
}

/**
 * meta_data_mem
 * 
 */
typedef struct meta_data_mem
{
	unsigned int proto_id;
	unsigned long long int addr_to_migrate;
	unsigned int batch_num;
	unsigned int batch_size;
} __attribute__ ((packed)) meta_data_mem_t;

static void
init_meta_data_mem(meta_data_mem_t* mm)
{
    mm->proto_id = 0;
}

/**
 * Type for meta data part in msg.
 */
typedef union bf_msg_meta {
    meta_basic_t basic; // to get proto_id.
    meta_node_t node;
    meta_umem_t umem;
    meta_data_cpu_t data_cpu;
    meta_data_mem_t data_mem;
} bf_msg_meta_t;

static void
init_msg_meta (bf_msg_meta_t* mm)
{
    memset (mm, 0, sizeof(bf_msg_meta_t));

    init_meta_basic (&mm->basic);
    init_meta_node (&mm->node);
    init_meta_umem (&mm->umem);
    init_meta_data_cpu (&mm->data_cpu);
    init_meta_data_mem (&mm->data_mem);
}

/**
 * Type of whole msg.
 */
typedef struct bf_msg {
    bf_msg_meta_t meta;
    char data[SIZE_SENDBUF_DATA];
} __attribute__ ((packed)) bf_msg_t;

void
bf_msg_init_msg (bf_msg_t* m)
{
    init_msg_meta(&m->meta);
    memset (m->data, 0, sizeof(m->data));
}

/**
 * Return protocol id with bf_msg_meta.
 */
unsigned int
bf_msg_meta_get_proto_id(bf_msg_meta_t* mm){
    return mm->basic.proto_id;
}

/**
 * Return protocol id with bf_msg.
 */
unsigned int
bf_msg_get_proto_id(bf_msg_t* m){
    return bf_msg_meta_get_proto_id((bf_msg_meta_t*)&(m->meta));
}

/**
 * Set protocol id of bf_msg.
 */
void
bf_msg_set_proto_id(bf_msg_t* m, unsigned int id){
    m->meta.basic.proto_id = id;
}

/**
 * Set protocol id of bf_msg_meta.
 */
void
bf_msg_meta_set_proto_id(bf_msg_meta_t* mm, unsigned int id){
    mm->basic.proto_id = id;
}

/**
 * Format the meta part of a network message.
 */
void
bf_msg_format_meta_node(bf_msg_meta_t* mm, unsigned long long int total_mem_size, unsigned int umem_count)
{
    init_msg_meta(mm);
    mm->node.proto_id = PROTO_ID_META_NODE;
    mm->node.total_mem_size = total_mem_size;
    mm->node.umem_count = umem_count;
}

void
bf_msg_format_meta_umem(bf_msg_meta_t* mm, unsigned int umem_id, unsigned long long int umem_size, unsigned long long int start_addr)
{
    init_msg_meta(mm);
    mm->umem.proto_id = PROTO_ID_META_UMEM;
    mm->umem.umem_id = umem_id;
    mm->umem.umem_size = umem_size;
    mm->umem.start_addr = start_addr;
}

void
bf_msg_format_meta_data_cpu(bf_msg_meta_t* mm)
{
    init_msg_meta(mm);
    mm->data_cpu.proto_id = PROTO_ID_META_DATA_CPU;
}

void
bf_msg_format_meta_data_mem(bf_msg_meta_t* mm)
{
    init_msg_meta(mm);
    mm->data_mem.proto_id = PROTO_ID_META_DATA_MEM;
}

/**
 * Store buf into bf_msg_t type (msg).
 */
void
bf_msg_decode(char* buf, bf_msg_t* msg)
{
    memcpy(&(msg->meta), &buf[0], sizeof(bf_msg_meta_t)); // get meta
    memcpy(msg->data, &buf[sizeof(bf_msg_meta_t)], SIZE_SENDBUF_DATA);	// get data
}


#ifdef KERN_CONT
/**
 * Print msg in kernel.
 */
void
bf_msg_printk(bf_msg_t* msg)
{
    unsigned char *char_p = 0;
    int i;

    // print msg meta part.
    pr_info("<msg meta>:\n");
    char_p = (unsigned char*)&(msg->meta);
    for (i = 0; i< sizeof(bf_msg_meta_t); i++){
	if (i%32 == 0) {
	    pr_info(" ");
	    printk (KERN_CONT "%d: ", i);
	}
	if (i%8 == 0){
	    printk (KERN_CONT " ");
	}
	printk(KERN_CONT "%02x|", char_p[i]);
    }

    // print msg data part.
    pr_info("<msg data>:\n");
    for (i = 0; i< SIZE_SENDBUF_DATA; i++){
	if (i%32 == 0) {
	    pr_info(" ");
	    printk (KERN_CONT "%d: ", i);
	}
	if (i%8 == 0) printk (KERN_CONT " ");
	printk(KERN_CONT "%02x|", (unsigned char)(msg->data[i]));
    }
    pr_info("<msg> printing done.\n");

}
#endif /* KERN_CONT */


#endif	/* __BIFORM_MSG_PROTOCOL_H */
