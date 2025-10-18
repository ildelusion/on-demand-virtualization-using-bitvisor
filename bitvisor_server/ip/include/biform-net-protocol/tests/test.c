#include <stdio.h>
#include "../msg_protocol.h"

;

int main()
{
	struct meta_node mn1;
	unsigned char* mn1_p;
	int i = 0;

	char msg_buf[SIZE_SENDBUF_DATA + sizeof(bf_msg_meta_t)];

	mn1.proto_id = 0x12345678;
	mn1.total_mem_size = 0xffffeeeeddddcccc;
	mn1.umem_count = 0x9ab9ab9a;
	mn1_p = &mn1;
	printf ("hihih\n");
	printf ("size of meta_node:%ld\n", sizeof(mn1));
	printf ("size of proto_id:%ld\n", sizeof(mn1.proto_id));
	printf ("size of total_mem_size:%ld\n", sizeof(mn1.total_mem_size));
	printf ("size of umem_count:%ld\n", sizeof(mn1.umem_count));

	for (i = 0; i< sizeof(mn1); i++){
		printf ("%d:%02hhx\n", i, (unsigned char)*(mn1_p+i));
	}

	printf ("size of meta_basic_t:%d meta_node_t:%d meta_umem_t:%d, data_cpu_t:%d, data_mem_t:%d\n",
			sizeof(meta_basic_t), sizeof(meta_node_t), sizeof(meta_umem_t),
			sizeof (meta_data_cpu_t), sizeof(meta_data_mem_t));

	printf("size of bf_msg_meta_t:%d\n", sizeof(bf_msg_meta_t));
	printf("size of bf_msg_t:%d\n", sizeof(bf_msg_t));
	printf("size of msg_buf:%d\n", sizeof(msg_buf));

	bf_msg_t msg;
	mn1_p = &msg;
	bf_msg_init_msg(&msg);
	msg.meta.basic.proto_id=0x55;

	for (i = 0; i< sizeof (bf_msg_t); i++){
		if (i%32 == 0) printf("\n%d\t: ", i);
		if (i%8 == 0) printf(" ");
	//	printf("%02x", (unsigned char)msg.data[i]);
		printf("%02hhx", (unsigned char)*(mn1_p+i));
	}

	printf("\n");


	bf_msg_set_proto_id(&msg, 11);

	printf("get proto_id:%d\n", bf_msg_get_proto_id(&msg));

	bf_msg_meta_set_proto_id(&(msg.meta), 10);
	printf("get proto_id:%d\n", bf_msg_meta_get_proto_id(&(msg.meta)));

	return 0;
}
