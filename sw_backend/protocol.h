#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__
#include <stdint.h>

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
/* Type Definition */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
/* Packet Structure (Protocol) */
struct task_struct {
	u8 msg_head[2];
	u16 msg_error;
	u16 msg_type;
	u16 data_length;
	u8 data[52];
	u16 crc;
	u8 msg_tail[2];
} __attribute__((packed));
/* Function Prototype */
int usb_request(int dev_fd, struct task_struct *task, u16 msg_type, const u8 *data, u16 data_len);
int usb_recv(int dev_fd, struct task_struct *task);
int usb_err_check(struct task_struct *task);

#endif
