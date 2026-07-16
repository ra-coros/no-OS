/*******************************************************************************
 *   @file   boardsupport.c
 *   @brief  Implementation for the Board Support Package.
 *   @author Ra Coros (ra.coros@analog.com)
 ********************************************************************************
 * Copyright 2026(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/
#include <string.h>
#include <errno.h>
#include "boardsupport.h"
#include "capi_gpio.h"
#include "capi_irq.h"
#include "stm32_capi_irq.h"
#include "capi_time.h"

#define RESET_DELAY       (1)
#define AFTER_RESET_DELAY (100)

static struct bsp_desc bsp_instance;

int bsp_system_init(struct bsp_desc **desc)
{
	int ret;
	struct bsp_desc *d = &bsp_instance;

	if (!desc)
		return -EINVAL;

	memset(d, 0, sizeof(*d));

	stm32_init();

	/* IRQ controller — must be first, all drivers depend on it */
	struct stm32_capi_irq_extra_config irq_extra = {
		.priority_grouping = NVIC_PRIORITYGROUP_4,
		.default_preempt_priority = 0,
		.default_sub_priority = 0,
	};
	struct capi_irq_config irq_cfg = {
		.irq_ctrl_id = 0,
		.extra = &irq_extra,
	};
	ret = capi_irq_init(&irq_cfg);
	if (ret)
		return ret;

	/* DMA */
	struct bsp_dma_init_param dma_param = { .num_channels = 2 };
	ret = dma_init(&d->dma, &dma_param);
	if (ret)
		return ret;

	/* GPIO */
	ret = gpio_init(&d->gpio);
	if (ret)
		goto err_dma;

	/* SPI */
	struct bsp_spi_init_param spi_param = {
		.dma_handle = d->dma->handle,
	};
	ret = spi_init(&d->spi, &spi_param);
	if (ret)
		goto err_gpio;

	/* UART */
	struct bsp_uart_init_param uart_param = { .baudrate = 115200 };
	ret = uart_init(&d->uart, &uart_param);
	if (ret)
		goto err_spi;

	bsp_set_debug_uart(d->uart);

	*desc = d;
	return 0;

err_spi:
	spi_remove(d->spi);
err_gpio:
	gpio_remove(d->gpio);
err_dma:
	dma_remove(d->dma);
	return ret;
}

void bsp_system_remove(struct bsp_desc *desc)
{
	if (!desc)
		return;

	uart_remove(desc->uart);
	spi_remove(desc->spi);
	gpio_remove(desc->gpio);
	dma_remove(desc->dma);
}

/* --- Reset --- */

void bsp_hw_reset(struct bsp_desc *desc, bool set)
{
	(void)set;
	capi_gpio_pin_set_value(&desc->gpio->eth_reset, CAPI_GPIO_LOW);
	capi_wait_ms(RESET_DELAY);
	capi_gpio_pin_set_value(&desc->gpio->eth_reset, CAPI_GPIO_HIGH);
	capi_wait_ms(AFTER_RESET_DELAY);
}

/* --- LEDs --- */

static void bsp_led_set(struct capi_gpio_pin *pin, bool on)
{
	capi_gpio_pin_set_value(pin, on ? CAPI_GPIO_LOW : CAPI_GPIO_HIGH);
}

static void bsp_led_toggle(struct capi_gpio_pin *pin)
{
	uint8_t val;
	capi_gpio_pin_get_value(pin, &val);
	capi_gpio_pin_set_value(pin, val ? CAPI_GPIO_LOW : CAPI_GPIO_HIGH);
}

void bsp_heart_beat(struct bsp_desc *desc)
{
	bsp_led_toggle(&desc->gpio->led3);
}

void bsp_heart_beat_led(struct bsp_desc *desc, bool on)
{
	bsp_led_set(&desc->gpio->led3, on);
}

void bsp_error_led(struct bsp_desc *desc, bool on)
{
	bsp_led_set(&desc->gpio->led2, on);
}

void bsp_func_led1(struct bsp_desc *desc, bool on)
{
	bsp_led_set(&desc->gpio->led1, on);
}

void bsp_func_led1_toggle(struct bsp_desc *desc)
{
	bsp_led_toggle(&desc->gpio->led1);
}

void bsp_func_led2(struct bsp_desc *desc, bool on)
{
	bsp_led_set(&desc->gpio->led4, on);
}

void bsp_func_led2_toggle(struct bsp_desc *desc)
{
	bsp_led_toggle(&desc->gpio->led4);
}

void bsp_led_toggle_all(struct bsp_desc *desc)
{
	bsp_led_toggle(&desc->gpio->led1);
	bsp_led_toggle(&desc->gpio->led2);
	bsp_led_toggle(&desc->gpio->led3);
	bsp_led_toggle(&desc->gpio->led4);
#if !defined(EVAL_ADIN1110EBZ)
	bsp_led_toggle(&desc->gpio->led5);
#endif
}

/* --- Config pins --- */

void bsp_get_config_pins(struct bsp_desc *desc, uint16_t *value)
{
	uint8_t val;
	uint16_t result = 0;

	capi_gpio_pin_get_value(&desc->gpio->cfg0, &val);
	result |= (uint16_t)val << 0;

	capi_gpio_pin_get_value(&desc->gpio->cfg1, &val);
	result |= (uint16_t)val << 1;

	capi_gpio_pin_get_value(&desc->gpio->cfg2, &val);
	result |= (uint16_t)val << 2;

	capi_gpio_pin_get_value(&desc->gpio->cfg3, &val);
	result |= (uint16_t)val << 3;

	*value = result;
}

/* --- IRQ --- */

uint32_t bsp_register_irq_callback(struct bsp_desc *desc,
				   capi_isr_callback_t callback, void *param)
{
	return gpio_register_irq_callback(desc->gpio, callback, param);
}

void bsp_disable_irq(struct bsp_desc *desc)
{
	(void)desc;
	capi_irq_disable(ETH_INT_N_IRQn);
}

void bsp_enable_irq(struct bsp_desc *desc)
{
	(void)desc;
	capi_irq_enable(ETH_INT_N_IRQn);
}

/* --- Timing --- */

uint32_t bsp_sys_now(void)
{
	return HAL_GetTick();
}

/* --- UART / Debug --- */

static struct capi_uart_handle *debug_uart_handle;

void bsp_set_debug_uart(struct bsp_uart_desc *uart)
{
	debug_uart_handle = uart ? uart->handle : NULL;
}

uint32_t msg_read(char *ptr, uint32_t size)
{
	if (!ptr || !debug_uart_handle)
		return 1;

	return (uint32_t)capi_uart_receive(debug_uart_handle, (uint8_t *)ptr,
					   size);
}

uint32_t msg_write(char *ptr)
{
	if (!ptr || !debug_uart_handle)
		return 1;

	return (uint32_t)capi_uart_transmit(debug_uart_handle, (uint8_t *)ptr,
					    strlen(ptr));
}

char aDebugString[150u];
char aDebugChar;

void common_fail(char *failure_reason)
{
	char fail[] = "Failed: ";
	char term[] = "\n\r";

	msg_write(fail);
	msg_write(failure_reason);
	msg_write(term);
}

void common_perf(char *info_string)
{
	char term[] = "\n\r";

	msg_write(info_string);
	msg_write(term);
}

void common_perf_read(char *info_string)
{
	msg_read(info_string, 1);
}

uint32_t common_readline(char *buffer, uint32_t max_len)
{
	uint32_t idx = 0;
	char c;

	while (idx < max_len - 1) {
		if (msg_read(&c, 1) == HAL_OK) {
			if (c == '\n' || c == '\r') {
				if (c == '\r') {
					char next;
					msg_read(&next, 1);
				}
				break;
			}
			buffer[idx++] = c;
		}
	}
	buffer[idx] = '\0';
	return idx;
}

int32_t common_readinteger(void)
{
	char buffer[16];

	if (common_readline(buffer, sizeof(buffer)) > 0)
		return atoi(buffer);

	return 0;
}
