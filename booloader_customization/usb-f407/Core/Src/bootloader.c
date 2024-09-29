/*
 * bootloader.c
 *
 *  Created on: Sep 26, 2024
 *      Author: dinhnamuet
 */
/*
10 0000 00 0000052025680108AD660108B5660108 F5

Record Type = {
	00: Write data
	01: End of Record
	02: Extended Segment Address
	03: Start Segment Address
	04: Extended Linear Address
	05: Start Linear Address
}
*/
#include "bootloader.h"
#include <string.h>

void start_boot_checking(struct boot_button *button) {
	if (HAL_GPIO_ReadPin(button->port, button->pin) == BOOTLOADER_MODE) {
		//do nothing
	} else {
		goto_application(PROGRAM_ADDRESS);
	}
}

/* Run application */
void __attribute__((noreturn)) goto_application(u32 p_addr) {
	/* Turn off Peripheral, Clear Interrupt Flag*/
	HAL_RCC_DeInit();
	/* Clear Pending Interrupt Request, turn off System Tick*/
	HAL_DeInit();
	/* Turn off fault harder*/
	SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
			SCB_SHCSR_BUSFAULTENA_Msk |
			SCB_SHCSR_MEMFAULTENA_Msk);
	/* Set Vector Table */
	SCB->VTOR = p_addr;

	__set_MSP(*((volatile u32 *) p_addr));
	void (*reset_handler)(void) = (void *)(*((volatile u32 *) (p_addr + 4U)));
	reset_handler();
	for(;;) {
		__NOP();
	}
}

/* Handle HEX Frame */
err_t hex_line_handler(const u8 *hex_line, u32 length) {
	u8 type, checksum;
	static addr_t flash;
	type = hex_line[INDEX_TYPE];
	checksum = 0x00;
	for (int i = 0; i < length; i++) {
		checksum += hex_line[i];
	}
	if (checksum) {
		return HEX_CS_FAILED;
	}
	switch (type) {
		case DATA_RECORD:
			flash.addr.offset = (u16)(hex_line[INDEX_ADDR] << 8) | (u16)hex_line[INDEX_ADDR + 1];
			if (flash_write(flash.address, &hex_line[INDEX_DATA], hex_line[INDEX_LEN]) < 0) {
				return HEX_WR_FAILED;
			}
			break;
		case END_OF_RECORD:
			break;
		case EXTENDED_SEG_ADDR:
			break;
		case START_SEG_ADDR:
			break;
		case EXTENDED_LINEAR_ADDR:
			flash.addr.base = (u16)(hex_line[INDEX_DATA] << 8) | (u16)hex_line[INDEX_DATA + 1];
			break;
		case START_LINEAR_ADDR:
			break;
		default:
			return HEX_INVALID;
	}
	return HEX_SUCCESS;
}
