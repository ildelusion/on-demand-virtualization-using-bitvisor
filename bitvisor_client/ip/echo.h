#ifndef ECHO_H
#define ECHO_H

struct mig_mem_info {
	unsigned long * mem;
	long total_send_num;
};

void echo_server_init (int port);
int echo_client_send (unsigned long* mem, long total_num_of_send);
void echo_client_send_mem (void);
void echo_client_init (int *ipaddr, int port);

#endif	/* ECHO_H */
