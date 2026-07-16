/*******************************************************************************
 *   @file   usart.c
 *   @brief  USART driver implementation using CAPI.
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
#include "usart.h"
#include "capi_irq.h"
#include "stm32_capi_uart.h"
#include "stm32_hal.h"

static struct bsp_uart_desc uart_instance;

static void uart_irq_handler(void *arg)
{
	struct bsp_uart_desc *d = (struct bsp_uart_desc *)arg;

	if (d && d->handle)
		capi_uart_isr(d->handle);
}

int uart_init(struct bsp_uart_desc **desc, struct bsp_uart_init_param *param)
{
	int ret;
	struct bsp_uart_desc *d = &uart_instance;

	if (!desc || !param)
		return -EINVAL;

	memset(d, 0, sizeof(*d));

	struct capi_uart_line_config line_cfg = {
		.baudrate     = param->baudrate,
		.size         = CAPI_UART_DATA_BITS_8,
		.parity       = CAPI_UART_PARITY_NONE,
		.stop_bits    = CAPI_UART_STOP_1_BIT,
		.flow_control = CAPI_UART_FLOW_CONTROL_NONE,
	};

	struct capi_uart_config uart_cfg = {
		.identifier  = (uint32_t)(uintptr_t)USART1,
		.dma_handle  = NULL,
		.clk_freq_hz = 0,
		.line_config = &line_cfg,
		.extra       = NULL,
		.ops         = &stm32_capi_uart_ops,
	};

	ret = capi_uart_init(&d->handle, &uart_cfg);
	if (ret)
		return ret;

	ret = capi_irq_set_priority(USART1_IRQn, 1);
	if (ret)
		return ret;
	capi_irq_connect(USART1_IRQn, uart_irq_handler, d);
	ret = capi_irq_enable(USART1_IRQn);
	if (ret)
		return ret;

	*desc = d;
	return 0;
}

void uart_remove(struct bsp_uart_desc *desc)
{
	if (!desc)
		return;

	capi_irq_disable(USART1_IRQn);

	if (desc->handle) {
		capi_uart_deinit(desc->handle);
		desc->handle = NULL;
	}
}

int uart_transmit(struct bsp_uart_desc *desc, uint8_t *buf, uint32_t len)
{
	if (!desc || !desc->handle)
		return -EINVAL;

	return capi_uart_transmit(desc->handle, buf, len);
}

int uart_receive(struct bsp_uart_desc *desc, uint8_t *buf, uint32_t len)
{
	if (!desc || !desc->handle)
		return -EINVAL;

	return capi_uart_receive(desc->handle, buf, len);
}
