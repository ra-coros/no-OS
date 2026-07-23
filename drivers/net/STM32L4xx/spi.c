/*******************************************************************************
 *   @file   spi.c
 *   @brief  SPI driver implementation using CAPI.
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
#include <errno.h>
#include <string.h>
#include "spi.h"
#include "capi_irq.h"
#include "stm32_capi_spi.h"
#include "stm32_capi_gpio.h"
#include "bsp_config.h"

#define GPIO_PIN_NUM(pin_mask) (__builtin_ctz(pin_mask))

static struct bsp_spi_desc spi_instance;

static void spi_irq_handler(void *arg)
{
	struct bsp_spi_desc *d = (struct bsp_spi_desc *)arg;

	if (d && d->handle)
		capi_spi_isr(d->handle);
}

int spi_init(struct bsp_spi_desc **desc, struct bsp_spi_init_param *param)
{
	int ret;
	struct bsp_spi_desc *d = &spi_instance;

	if (!desc || !param)
		return -EINVAL;

	memset(d, 0, sizeof(*d));

	ETH_SPI_CLK_ENABLE();

	struct stm32_spi_extra_config spi_extra = {
		.hspi          = NULL,
		.chip_select_port = (uint32_t)(uintptr_t)ETH_SPI_SS_GPIO_Port,
		.get_input_clock = NULL,
		.alternate     = ETH_SPI_CLK_AF,
		.dma_handle    = param->dma_handle,
		.rxdma_ch_id   = 1,
		.txdma_ch_id   = 0,
		.irq_num       = ETH_SPI_IRQn,
	};

	struct capi_spi_config spi_cfg = {
		.ops           = &stm32_capi_spi_ops,
		.identifier    = (uint64_t)(uintptr_t)ETH_SPI,
		.dma_handle    = param->dma_handle,
		.three_pin_mode = false,
		.loopback      = false,
		.clk_freq_hz   = 0,
		.extra         = &spi_extra,
	};

	ret = capi_spi_init(&d->handle, &spi_cfg);
	if (ret)
		return ret;

	ret = capi_irq_set_priority(ETH_SPI_IRQn, 1);
	if (ret)
		return ret;
	capi_irq_connect(ETH_SPI_IRQn, spi_irq_handler, d);
	ret = capi_irq_enable(ETH_SPI_IRQn);
	if (ret)
		return ret;

	struct stm32_capi_gpio_port_config cs_gpio_cfg = {
		.mode      = GPIO_MODE_OUTPUT_PP,
		.speed     = GPIO_SPEED_FREQ_LOW,
		.pull      = GPIO_NOPULL,
		.alternate = 0,
	};

	struct capi_gpio_port_config cs_port_cfg = {
		.ops        = &stm32_capi_gpio_ops,
		.identifier = (uint64_t)(uintptr_t)ETH_SPI_SS_GPIO_Port,
		.num_pins   = 16,
		.flags      = NULL,
		.extra      = &cs_gpio_cfg,
	};

	ret = capi_gpio_port_init(&d->cs_pin.port_handle, &cs_port_cfg);
	if (ret)
		return ret;

	d->cs_pin.number      = GPIO_PIN_NUM(ETH_SPI_SS_Pin);
	d->cs_pin.flags       = CAPI_GPIO_ACTIVE_LOW;

	ret = capi_gpio_pin_set_direction(&d->cs_pin, CAPI_GPIO_OUTPUT);
	if (ret)
		return ret;

	ret = capi_gpio_pin_set_raw_value(&d->cs_pin, CAPI_GPIO_HIGH);
	if (ret)
		return ret;

	d->device.controller   = d->handle;
	d->device.max_speed_hz = 0;
	d->device.mode         = CAPI_SPI_MODE_0;
	d->device.native_cs    = 0;
	d->device.cs_gpio      = &d->cs_pin;
	d->device.cs_gpio_num  = 1;
	d->device.lsb_first    = false;
	d->device.extra        = NULL;

	*desc = d;
	return 0;
}

void spi_remove(struct bsp_spi_desc *desc)
{
	if (!desc)
		return;

	capi_irq_disable(ETH_SPI_IRQn);

	if (desc->cs_pin.port_handle)
		capi_gpio_port_deinit(&desc->cs_pin.port_handle);

	if (desc->handle) {
		capi_spi_deinit(desc->handle);
		desc->handle = NULL;
	}

	ETH_SPI_CLK_DISABLE();
}

int spi_register_callback(struct bsp_spi_desc *desc,
			  capi_spi_callback_t callback, void *arg)
{
	if (!desc || !desc->handle)
		return -EINVAL;

	return capi_spi_register_callback(desc->handle, callback, arg);
}
