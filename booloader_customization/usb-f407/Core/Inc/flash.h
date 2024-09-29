/*
 * flash.h
 *
 *  Created on: Sep 26, 2024
 *      Author: dinhnamuet
 */

#ifndef INC_FLASH_H_
#define INC_FLASH_H_
#include "stm32f4xx_hal.h"

HAL_StatusTypeDef flash_erase(u32 base_sector, u32 num_sector);
HAL_StatusTypeDef flash_write(u32 address, const u8 *data, u32 len);
int flash_read(u32 address, void *desc, u32 length);

#endif /* INC_FLASH_H_ */
