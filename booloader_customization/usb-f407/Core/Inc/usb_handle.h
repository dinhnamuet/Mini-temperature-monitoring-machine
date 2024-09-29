/*
 * usb_handle.h
 *
 *  Created on: Sep 25, 2024
 *      Author: dinhnamuet
 */

#ifndef INC_USB_HANDLE_H_
#define INC_USB_HANDLE_H_
#include "stm32f4xx.h"
#include "task_list.h"

/* Message type */
#define MSG_REQUEST_DATA	0x2002
#define MSG_GOTO_APP		0x1979
#define MSG_PROGRAM_DATA    0x2001
/* Error Code */
#define	MSG_SUCCESS			0x3230
#define MSG_INVALID			0x3231
#define MSG_WFORMAT			0x3232
#define MSG_FAILED			0x3233
#define MSG_WRONG_CRC		0x3234

int usb_handle_packet(struct task_struct *task);
int usb_response_pkt(struct task_struct *task);

#endif /* INC_USB_HANDLE_H_ */
