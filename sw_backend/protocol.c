#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include "protocol.h"
#include "CRC.h"

int usb_request(int dev_fd, struct task_struct *task, u16 msg_type, const u8 *data, u16 data_len) {
    int ret;
    u8 *wdata = (u8 *)task;
    task->msg_head[0]       = 0xFA;
    task->msg_head[1]       = 0xFB;
    task->msg_error         = MSG_SUCCESS;
    task->msg_type          = msg_type;
    task->data_length       = data_len;
    memcpy(task->data, data, data_len);
    task->crc               = CRC_CalculateCRC16(task->data, task->data_length);
    task->msg_tail[0]       = 0xFC;
    task->msg_tail[1]       = 0xFD;
    
    ret = write(dev_fd, task, sizeof(struct task_struct));
    return ret;
}
int usb_recv(int dev_fd, struct task_struct *task) {
    return read(dev_fd, task, sizeof(struct task_struct));
}

int usb_err_check(struct task_struct *task) {
	if (task->msg_head[0] != 0xFA || task->msg_head[1] != 0xFB || task->msg_tail[0] != 0xFC || task->msg_tail[1] != 0xFD) {
		return -1;
	} else if (CRC_CalculateCRC16(task->data, task->data_length) != task->crc) {
		return -1;
	} else if (task->msg_error != MSG_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}
