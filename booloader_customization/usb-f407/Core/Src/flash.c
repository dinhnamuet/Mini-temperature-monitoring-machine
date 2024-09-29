/*
 * flash.c
 *
 *  Created on: Sep 26, 2024
 *      Author: dinhnamuet
 */
#include "flash.h"

HAL_StatusTypeDef flash_erase(u32 base_sector, u32 num_sector) {
	HAL_StatusTypeDef res = HAL_ERROR;
	FLASH_EraseInitTypeDef erase;
	u32 page_err;

	erase.Banks			= FLASH_BANK_1;
	erase.Sector		= base_sector;
	erase.NbSectors		= num_sector;
	erase.TypeErase 	= FLASH_TYPEERASE_SECTORS;
	erase.VoltageRange	= FLASH_VOLTAGE_RANGE_3;

	HAL_FLASH_Unlock();
	res = HAL_FLASHEx_Erase(&erase, &page_err);
	HAL_FLASH_Lock();
	return res;
}
HAL_StatusTypeDef flash_write(u32 address, const u8 *data, u32 len) {
    HAL_StatusTypeDef res;
    HAL_FLASH_Unlock();
    for (int i = 0; i < len; i++) {
    	res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]);
    	if (res < 0) {
    		goto exit;
    	}
    }
exit:
    HAL_FLASH_Lock();
    return res;
}

int flash_read(u32 address, void *desc, u32 length) {
	u8 *ptr = (u8 *)desc;
	if (!desc) {
		return -1;
	}
	for (int i = 0; i < length; i++) {
		ptr[i] = *(volatile u8*) (address + i);
	}
	return 0;
}
