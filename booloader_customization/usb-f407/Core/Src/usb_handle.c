/*
 * usb_handle.c
 *
 *  Created on: Sep 25, 2024
 *      Author: nam
 */

#include "usb_handle.h"
#include "usbd_cdc_if.h"
#include "CRC.h"
#include <string.h>
#include "bootloader.h"

extern u16 adc_val;

int usb_handle_packet(struct task_struct *task) {
	struct task_struct response_task;
	response_task.msg_error = MSG_SUCCESS;
	int res;

	if (task->msg_head[0] != 0xFA || task->msg_head[1] != 0xFB || task->msg_tail[0] != 0xFC || task->msg_tail[1] != 0xFD) {
		response_task.msg_error = MSG_WFORMAT;
		response_task.data_length = 0;
		goto full_fill_and_response;
	} else if (CRC_CalculateCRC16(task->data, task->data_length) != task->crc) {
		response_task.msg_error = MSG_WRONG_CRC;
		response_task.data_length = 0;
		goto full_fill_and_response;
	}

	switch (task->msg_type) {
		case MSG_REQUEST_DATA:
			memcpy(response_task.data, &adc_val, sizeof(u16));
			response_task.data_length = sizeof(u16);
			adc_val++;
			break;
		case MSG_GOTO_APP:
			response_task.data_length = 0;
			break;
		case MSG_PROGRAM_DATA:
			if (hex_line_handler(task->data, task->data_length) != HEX_SUCCESS) {
				response_task.msg_error = MSG_FAILED;
			}
			response_task.data_length = 0;
			break;
		default:
			response_task.msg_error = MSG_INVALID;
			response_task.data_length = 0;
			break;
	}

full_fill_and_response:
	response_task.msg_head[0] = 0xFA;
	response_task.msg_head[1] = 0xFB;
	response_task.msg_type = task->msg_type;
	response_task.crc = CRC_CalculateCRC16(response_task.data, response_task.data_length);
	response_task.msg_tail[0] = 0xFC;
	response_task.msg_tail[1] = 0xFD;
	res = usb_response_pkt(&response_task);
	if (task->msg_type == MSG_GOTO_APP) {
		goto_application(PROGRAM_ADDRESS);
	}
	return res;
}

int usb_response_pkt(struct task_struct *task) {
	return CDC_Transmit_FS((u8 *)task, sizeof(struct task_struct));
}
