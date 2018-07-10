/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2018  Flirc Inc.
 * Written by Jason Kotzin <jasonkotzin@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "general.h"
#include "gdb_if.h"
#include "cdcacm.h"
#include "usbuart.h"

#include <libopencm3/sam/d/nvic.h>
#include <libopencm3/sam/d/port.h>
#include <libopencm3/sam/d/gclk.h>
#include <libopencm3/sam/d/pm.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/scb.h>

static struct gclk_hw clock = {
	.gclk0 = SRC_DFLL48M,
	.gclk1 = SRC_DFLL48M,
	.gclk2 = SRC_DFLL48M,
	.gclk3 = SRC_DFLL48M,
	.gclk4 = SRC_DFLL48M,
	.gclk5 = SRC_DFLL48M,
	.gclk6 = SRC_DFLL48M,
	.gclk7 = SRC_DFLL48M,
};

extern void trace_tick(void);

uint8_t running_status;
static volatile uint32_t time_ms;

void sys_tick_handler(void)
{
	if(running_status)
		gpio_toggle(LED_PORT, LED_IDLE_RUN);

	time_ms += 10;
}

uint32_t platform_time_ms(void)
{
	return time_ms;
}

static void usb_setup(void)
{
	/* Enable USB */
	INSERTBF(PM_APBBMASK_USB, 1, PM->apbbmask);

	/* enable clocking to usb */
	set_periph_clk(GCLK0, GCLK_ID_USB);
	periph_clk_en(GCLK_ID_USB, 1);

	gpio_config_special(PORTA, GPIO24, SOC_GPIO_PERIPH_G);
	gpio_config_special(PORTA, GPIO25, SOC_GPIO_PERIPH_G);

}

static uint32_t timing_init(void)
{
	uint32_t cal = 0;

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(480000);	/* Interrupt us at 10 Hz */
	systick_interrupt_enable();

	systick_counter_enable();
	return cal;
}

void platform_init(void)
{
	gclk_init(&clock);

	usb_setup();

	gpio_config_output(LED_PORT, LED_IDLE_RUN, 0);
	gpio_config_output(TMS_PORT, TMS_PIN, 0);
	gpio_config_output(TCK_PORT, TCK_PIN, 0);
	gpio_config_output(TDI_PORT, TDI_PIN, 0);

	/* enable both input and output with pullup disabled by default */
	PORT_DIRSET(SWDIO_PORT) = SWDIO_PIN;
	PORT_PINCFG(SWDIO_PORT, SWDIO_PIN_NUM) |= GPIO_PINCFG_INEN | GPIO_PINCFG_PULLEN;
	gpio_clear(SWDIO_PORT, SWDIO_PIN);

	/* configure swclk_pin as output */
	gpio_config_output(SWCLK_PORT, SWCLK_PIN, 0);
	gpio_clear(SWCLK_PORT, SWCLK_PIN);

	gpio_config_input(TDO_PORT, TDO_PIN, 0);
	gpio_config_output(SRST_PORT, SRST_PIN, GPIO_OUT_FLAG_DEFAULT_HIGH);
	gpio_set(SRST_PORT, SRST_PIN);

	timing_init();

	//nvic_enable_irq(NVIC_UART0_IRQ);
	//usbuart_init();

	cdcacm_init();
}

void platform_srst_set_val(bool assert)
{
	volatile int i;
	if (assert) {
		gpio_clear(SRST_PORT, SRST_PIN);
		for(i = 0; i < 10000; i++) asm("nop");
	} else {
		gpio_set(SRST_PORT, SRST_PIN);
	}
}

bool platform_srst_get_val(void)
{
	return gpio_get(SRST_PORT, SRST_PIN) == 0;
}

void platform_delay(uint32_t ms)
{
	platform_timeout timeout;
	platform_timeout_set(&timeout, ms);
	while (!platform_timeout_is_expired(&timeout));
}

const char *platform_target_voltage(void)
{
	return "not supported";
}

char *serialno_read(char *s)
{
        int i;
	volatile uint32_t unique_id = *(volatile uint32_t *)0x0080A00C +
		*(volatile uint32_t *)0x0080A040 +
		*(volatile uint32_t *)0x0080A044 +
		*(volatile uint32_t *)0x0080A048;

        /* Fetch serial number from chip's unique ID */
        for(i = 0; i < 8; i++) {
                s[7-i] = ((unique_id >> (4*i)) & 0xF) + '0';
        }

        for(i = 0; i < 8; i++)
                if(s[i] > '9')
                        s[i] += 'A' - '9' - 1;
	s[8] = 0;

	return s;
}

void platform_request_boot(void)
{
}