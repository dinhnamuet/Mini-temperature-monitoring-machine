/*
 * led_bootloader.c
 *
 *  Created on: Sep 29, 2024
 *      Author: dinhnamuet
 */

#include "led_bootloader.h"

void led_ctrl(struct led *led) {
	if (led->counter >= led->blynk_period_ms) {
		led->port->ODR ^= led->pin;
		led_set_counter(led, 0);
	}
}
void led_increase_counter(struct led *led) {
	led->counter ++;
}
void led_set_counter(struct led *led, u64 value) {
	led->counter = value;
}
u64 led_get_counter(struct led *led) {
	return led->counter;
}
void led_set_blynk_period(struct led *led, u64 ms) {
	led->blynk_period_ms = ms;
}
u64 led_get_blynk_period(struct led *led) {
	return led->blynk_period_ms;
}
