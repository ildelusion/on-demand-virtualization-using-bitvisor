#ifndef ECHO_CLIENT_H
#define ECHO_CLIENT_H

u64 get_next_seq_idx (u64 prev_seq_idx);
int get_batch_size (u64 addr);
void select_mem_area(void);

#endif /* ECHO_CLIENT_H */
