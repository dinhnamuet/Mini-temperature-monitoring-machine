/*
 * bootloader.h
 *
 *  Created on: Sep 26, 2024
 *      Author: dinhnamuet
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include "stm32f4xx_hal.h"
#include "flash.h"
/* HEX Frame */
#define INDEX_LEN				0x00
#define INDEX_ADDR				0x01
#define INDEX_TYPE				0x03
#define INDEX_DATA				0x04
/* Data Record Type */
#define DATA_RECORD				0x00
#define END_OF_RECORD			0x01
#define EXTENDED_SEG_ADDR		0X02
#define START_SEG_ADDR			0X03
#define EXTENDED_LINEAR_ADDR	0X04
#define START_LINEAR_ADDR		0X05
/* PROGRAM ADDR */
#define PROGRAM_ADDRESS			0x08020000UL
/* BOOT OPTIONS */
#define BOOTLOADER_MODE			GPIO_PIN_RESET
#define APPLICATION_MODE		GPIO_PIN_SET
/* Address Management */
typedef union {
	u32 address;
	struct addrs {
		u16 offset;
		u16 base;
	} addr;
} addr_t;
/* Error Code */
typedef enum {
	HEX_SUCCESS,
	HEX_CS_FAILED,
	HEX_WR_FAILED,
	HEX_INVALID
} err_t;

struct boot_button {
	GPIO_TypeDef *port;
	u16 pin;
};

void start_boot_checking(struct boot_button *button);
void __attribute__((noreturn)) goto_application(u32 p_addr);
err_t hex_line_handler(const u8 *hex_line, u32 length);

#endif /* INC_BOOTLOADER_H_ */
