/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CORE_MMIO_H
#define __CORE_MMIO_H

#include <core/types.h>

#define TIME_MEASURE 1200

int aft_mig_in_mmio;
int print_guest_addr;
u32 t_sd_base;
u32 r_sd_base;
u32 virtio_send_queue;
u32 virtio_recv_queue;
u16 mig_flag_for_mac;
bool ept_recved;
bool req_from_ept;
unsigned long requested_addr;
//u64 t1, t2, t3, t4;
//u64 t_start[10];
//u64 t_margin[10];
//u64 t_guest_start[2];
//u64 t_exit;
//u64 t_exit_ept[10000];
//u64 t_vm[10000][2];
//u64 t_req[10000][2];
//bool ept_recv_flag;
//u64 t_vmrun[100000][2];
bool desc_flag;
//u64 t_system[10000];
//u64 t_vmrun_bf_mig[100000][2];

// 19-03-31 new time measuring
u64 mig_start_t;
//u32 vmrun_time[602];
u32 vmrun_time[1202];
u64 vm_exit_cnt;
u64 t_vmrun_sum;
u64 new_start_t;
bool first_new_start;
u32 t_area;
bool time_measure;
bool print_vmrunt;
//19-04-08 for full bandwidth
bool reception;
u64 sched_cnt[50];
u64 int_vector[256];

u64 num_back_receive;
u64 num_request_sent;
u64 sum_back_receive;
u64 num_recved_batch;
u64 request_sent_t, request_recved_t;
u64 sum_request_recv_t;

typedef int (*mmio_handler_t) (void *data, phys_t gphys, bool wr, void *buf,
			       uint len, u32 flags);

void *mmio_register (phys_t gphys, uint len, mmio_handler_t handler,
		     void *data);
void mmio_unregister (void *handle);
void *mmio_register_unlocked (phys_t gphys, uint len, mmio_handler_t handler,
			      void *data);

#endif
