/*
 * led_bootloader.h
 *
 *  Created on: Sep 29, 2024
 *      Author: dinhnamuet
 */

#ifndef INC_LED_BOOTLOADER_H_
#define INC_LED_BOOTLOADER_H_
#include "stm32f4xx_hal.h"

struct led {
	GPIO_TypeDef *port;
	u16 pin;
	volatile u64 counter;
	u64 blynk_period_ms;
};

void led_ctrl(struct led *led);
void led_increase_counter(struct led *led);
void led_set_counter(struct led *led, u64 value);
u64 led_get_counter(struct led *led);
void led_set_blynk_period(struct led *led, u64 ms);
u64 led_get_blynk_period(struct led *led);

#endif /* INC_LED_BOOTLOADER_H_ */
