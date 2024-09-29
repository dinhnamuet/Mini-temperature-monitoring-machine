/**
  ******************************************************************************
  * @file    task_list.h
  * @author  dinhnamuet
  * @brief   Header file of USB DFU.
  ******************************************************************************
  * @attention
  *
  * Implement Task queue for handle USB request
  *
  ******************************************************************************
*/
#ifndef __TASK_LIST_H__
#define __TASK_LIST_H__
#include "stm32f4xx_hal.h"
#include <stdatomic.h>

#define RX_BUFF_SIZE	64 /* Max USB EndPoint Size */

#pragma pack(1)
struct task_struct {
	u8 msg_head[2];
	u16 msg_error;
	u16 msg_type;
	u16 data_length;
	u8 data[52];
	u16 crc;
	u8 msg_tail[2];
};

struct task_queue {
	struct task_struct *task;
	uint32_t read_index;
	uint32_t write_index;
	atomic_int n_tasks;
	uint32_t queue_size;
};
#pragma pack()

void free_task_list(struct task_queue *queue);
int put_task_to_queue(struct task_queue *queue, struct task_struct task);
int queue_is_empty(struct task_queue *queue);
int init_queue(struct task_queue *queue, uint32_t size);
struct task_struct *get_new_task(struct task_queue *queue);

#endif /* TASK LIST */
